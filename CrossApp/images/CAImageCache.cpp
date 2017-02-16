

#include "CAImageCache.h"
#include "CAImage.h"
#include "ccMacros.h"
#include "basics/CAApplication.h"
#include "platform/platform.h"
#include "platform/CAFileUtils.h"
#include "support/ccUtils.h"
#include "basics/CAScheduler.h"
#include "renderer/CCGLProgram.h"
#include "renderer/CCGLProgramState.h"
#include "renderer/ccGLStateCache.h"
#include "support/CANotificationCenter.h"
#include "ccTypes.h"
#include "CCGL.h"
#include <errno.h>
#include <stack>
#include <string>
#include <cctype>
#include <queue>
#include <list>
#include <stdlib.h>
#include <pthread.h>
#include "basics/CAConfiguration.h"

NS_CC_BEGIN

#pragma CAImageCache

enum class _AsyncType
{
    AsyncImageType=0,
    AsyncStringType
}AsyncType;

typedef struct _AsyncStruct
{
    std::string                   filename;
    std::function<void(CAImage*)> function;
} AsyncStruct;

typedef struct _ImageInfo
{
    AsyncStruct *asyncStruct;
    CAImage        *image;
} ImageInfo;

static pthread_t s_loadingThread;

static pthread_mutex_t		s_SleepMutex;
static pthread_cond_t		s_SleepCondition;

static pthread_mutex_t      s_asyncStructQueueMutex;
static pthread_mutex_t      s_ImageInfoMutex;

static unsigned long s_nAsyncRefCount = 0;

static bool need_quit = false;

static std::queue<AsyncStruct*>* s_pAsyncStructQueue = NULL;

static std::queue<ImageInfo*>*   s_pImageQueue = NULL;

static CAImage::Format computeImageFormatType(string& filename)
{
    CAImage::Format ret = CAImage::Format::UNKOWN;

    if ((std::string::npos != filename.find(".jpg")) || (std::string::npos != filename.find(".jpeg")))
    {
        ret = CAImage::Format::JPG;
    }
    else if ((std::string::npos != filename.find(".png")) || (std::string::npos != filename.find(".PNG")))
    {
        ret = CAImage::Format::PNG;
    }
    else if ((std::string::npos != filename.find(".tiff")) || (std::string::npos != filename.find(".TIFF")))
    {
        ret = CAImage::Format::TIFF;
    }
    else if ((std::string::npos != filename.find(".webp")) || (std::string::npos != filename.find(".WEBP")))
    {
        ret = CAImage::Format::WEBP;
    }
   
    return ret;
}

static void loadImageData(AsyncStruct *pAsyncStruct)
{
    const char *filename = pAsyncStruct->filename.c_str();

    // compute image type
    CAImage::Format imageType = computeImageFormatType(pAsyncStruct->filename);
    if (imageType == CAImage::Format::UNKOWN)
    {
        //CCLOG("unsupported format %s",filename);
        //delete pAsyncStruct;
    }
    CAImage* image = new CAImage();
    if (image && !image->initWithImageFile(filename, false))
    {
        CC_SAFE_RELEASE(image);
        return;
    }
    // generate image info
    ImageInfo *pImageInfo = new ImageInfo();
    pImageInfo->asyncStruct = pAsyncStruct;
    pImageInfo->image = image;

    // put the image info into the queue
    pthread_mutex_lock(&s_ImageInfoMutex);
    s_pImageQueue->push(pImageInfo);
    pthread_mutex_unlock(&s_ImageInfoMutex);   
}

static void* loadImage(void* data)
{
    AsyncStruct *pAsyncStruct = NULL;

    while (true)
    {
        std::queue<AsyncStruct*> *pQueue = s_pAsyncStructQueue;
        pthread_mutex_lock(&s_asyncStructQueueMutex);// get async struct from queue
        if (pQueue->empty())
        {
            pthread_mutex_unlock(&s_asyncStructQueueMutex);
            if (need_quit) {
                break;
            }
            else {
                pthread_cond_wait(&s_SleepCondition, &s_SleepMutex);
                continue;
            }
        }
        else
        {
            pAsyncStruct = pQueue->front();
            pQueue->pop();
            pthread_mutex_unlock(&s_asyncStructQueueMutex);
            loadImageData(pAsyncStruct);
        }        
    }
    
    if( s_pAsyncStructQueue != NULL )
    {
        delete s_pAsyncStructQueue;
        s_pAsyncStructQueue = NULL;
        delete s_pImageQueue;
        s_pImageQueue = NULL;

        pthread_mutex_destroy(&s_asyncStructQueueMutex);
        pthread_mutex_destroy(&s_ImageInfoMutex);
        pthread_mutex_destroy(&s_SleepMutex);
        pthread_cond_destroy(&s_SleepCondition);
    }
    
    return 0;
}

CAImageCache* CAImageCache::getInstance()
{
    return CAApplication::getApplication()->getImageCache();
}

CAImageCache::CAImageCache()
{

}

CAImageCache::~CAImageCache()
{
    CCLOG("CrossApp: deallocing CAImageCache.");
    need_quit = true;
    pthread_cond_signal(&s_SleepCondition);
    m_mImages.clear();
}

const char* CAImageCache::description()
{
    return crossapp_format_string("<CAImageCache | Number of textures = %lu>", m_mImages.size()).c_str();
}

void CAImageCache::addImageAsync(const std::string& path, const std::function<void(CAImage*)>& function)
{
    std::string pathKey = FileUtils::getInstance()->fullPathForFilename(path);

    this->addImageFullPathAsync(pathKey, function);
}

void CAImageCache::addImageFullPathAsync(const std::string& path, const std::function<void(CAImage*)>& function)
{
    CAImage* image = m_mImages.at(path);

    if (image)
    {
        function(image);
        return;
    }
    
    // lazy init
    if (s_pAsyncStructQueue == NULL)
    {
        s_pAsyncStructQueue = new std::queue<AsyncStruct*>();
        s_pImageQueue = new std::queue<ImageInfo*>();
        
        pthread_mutex_init(&s_asyncStructQueueMutex, NULL);
        pthread_mutex_init(&s_ImageInfoMutex, NULL);
        pthread_mutex_init(&s_SleepMutex, NULL);
        pthread_cond_init(&s_SleepCondition, NULL);
        pthread_create(&s_loadingThread, NULL, loadImage, NULL);
        need_quit = false;
    }
    
    if (0 == s_nAsyncRefCount)
    {
        CAScheduler::getScheduler()->schedule(schedule_selector(CAImageCache::addImageAsyncCallBack), this, 0);
    }
    
    ++s_nAsyncRefCount;

    // generate async struct
    AsyncStruct *data = new AsyncStruct();
    data->filename = path;
    data->function = function;
    
    // add async struct into queue
    pthread_mutex_lock(&s_asyncStructQueueMutex);
    s_pAsyncStructQueue->push(data);
    pthread_mutex_unlock(&s_asyncStructQueueMutex);
    pthread_cond_signal(&s_SleepCondition);
}


void CAImageCache::addImageAsyncCallBack(float dt)
{
    // the image is generated in loading thread
    std::queue<ImageInfo*> *imagesQueue = s_pImageQueue;

    if (!imagesQueue->empty())
    {
        pthread_mutex_lock(&s_ImageInfoMutex);
        ImageInfo *pImageInfo = imagesQueue->front();
        imagesQueue->pop();
        pthread_mutex_unlock(&s_ImageInfoMutex);

        AsyncStruct *pAsyncStruct = pImageInfo->asyncStruct;
        CAImage *image = pImageInfo->image;
        image->premultipliedImageData();
        
        const char* filename = pAsyncStruct->filename.c_str();

        // cache the image
        m_mImages.erase(filename);
        m_mImages.insert(filename, image);
        image->release();

        if (pAsyncStruct->function)
        {
            pAsyncStruct->function(image);
        }
        
        delete pAsyncStruct;
        delete pImageInfo;

        --s_nAsyncRefCount;
        if (0 == s_nAsyncRefCount)
        {
            CAScheduler::getScheduler()->unschedule(schedule_selector(CAImageCache::addImageAsyncCallBack), this);
        }
    }
}

CAImage* CAImageCache::addImage(const std::string& path)
{
    if (path.empty())
    {
        return NULL;
    }
    
    CAImage* image = NULL;
    
    //pthread_mutex_lock(m_pDictLock);

    image = m_mImages.at(path);

    if (!image)
    {
        std::string lowerCase(path);
        for (unsigned int i = 0; i < lowerCase.length(); ++i)
        {
            lowerCase[i] = tolower(lowerCase[i]);
        }
        
        do
        {
            image = new CAImage();
            if(image != NULL && image->initWithImageFile(path.c_str()))
            {
                m_mImages.erase(path);
                m_mImages.insert(path, image);
                image->release();
            }
            else
            {
                CC_SAFE_DELETE(image);
            }

        } while (0);
    }
    
    //pthread_mutex_unlock(m_pDictLock);
    return image;
}

bool CAImageCache::reloadImage(const std::string& fileName)
{
    if (fileName.size() == 0)
    {
        return false;
    }
    
    CAImage*  image = m_mImages.at(fileName);
    
    bool ret = false;
    if (! image)
    {
        image = this->addImage(fileName);
        ret = (image != NULL);
    }

    return ret;
}

// ImageCache - Remove

void CAImageCache::removeAllImages()
{
    m_mImages.clear();
}

void CAImageCache::removeUnusedImages()
{
    if (!m_mImages.empty())
    {
        CAMap<std::string, CAImage*> images = CAMap<std::string, CAImage*>(m_mImages);
        
        m_mImages.clear();
        
        CAMap<std::string, CAImage*>::iterator itr;
        for (itr=images.begin(); itr!=images.end(); itr++)
        {
            CC_CONTINUE_IF(itr->second->retainCount() == 1);
            m_mImages.insert(itr->first, itr->second);
        }
        images.clear();
    }
}

void CAImageCache::setImageForKey(CAImage* image, const std::string& key)
{
    CC_RETURN_IF(!image);
    m_mImages.insert(key, image);
}

void CAImageCache::removeImage(CAImage* image)
{
    CC_RETURN_IF(!image);
    if (!m_mImages.empty())
    {
        CAMap<std::string, CAImage*> images = CAMap<std::string, CAImage*>(m_mImages);
        
        m_mImages.clear();
        
        CAMap<std::string, CAImage*>::iterator itr;
        for (itr=images.begin(); itr!=images.end(); itr++)
        {
            CC_CONTINUE_IF(itr->second->isEqual(image));
            m_mImages.insert(itr->first, itr->second);
        }
        images.clear();
    }
    CCLog("CAImageCache:: %ld", m_mImages.size());
}

void CAImageCache::removeImageForKey(const std::string& imageKeyName)
{
    m_mImages.erase(imageKeyName);
}

CAImage* CAImageCache::imageForKey(const std::string& key)
{
    return m_mImages.at(key);
}

const std::string& CAImageCache::getImageFilePath(CAImage* image)
{
    for (CAMap<std::string, CAImage*>::iterator itr=m_mImages.begin();
         itr!=m_mImages.end();
         itr++)
    {
        if (itr->second->isEqual(image))
        {
            return itr->first;
            break;
        }
    }
}

void CAImageCache::reloadAllImages()
{
    CAImage::reloadAllImages();
}

void CAImageCache::dumpCachedImageInfo()
{
    unsigned int count = 0;
    unsigned int totalBytes = 0;

    CAMap<std::string, CAImage*>::iterator itr;
    for (itr=m_mImages.begin(); itr!=m_mImages.end(); itr++)
    {
        CAImage* image = itr->second;
        unsigned int bpp = image->bitsPerPixelForFormat();
        // Each texture takes up width * height * bytesPerPixel bytes.
        unsigned int bytes = image->getPixelsWide() * image->getPixelsHigh() * bpp / 8;
        totalBytes += bytes;
        count++;
        CCLog("CrossApp: \"%s\" rc=%lu id=%lu %lu x %lu @ %ld bpp => %lu KB",
              itr->first.c_str(),
              (long)image->retainCount(),
              (long)image->getName(),
              (long)image->getPixelsWide(),
              (long)image->getPixelsHigh(),
              (long)bpp,
              (long)bytes / 1024);

    }

    CCLog("CrossApp: CAImageCache dumpDebugInfo: %ld images, for %lu KB (%.2f MB)", (long)count, (long)totalBytes / 1024, totalBytes / (1024.0f*1024.0f));
}



#pragma CAImageAtlas

CAImageAtlas::CAImageAtlas()
:m_pIndices(nullptr)
,m_bDirty(false)
,m_pImage(nullptr)
,m_pQuads(nullptr)
{}

CAImageAtlas::~CAImageAtlas()
{
#if CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID
    CANotificationCenter::getInstance()->removeObserver(this, EVENT_COME_TO_FOREGROUND);
#endif
    
    CCLOG("CrossApp: CAImageAtlas deallocing %p.", this);
    
    CC_SAFE_FREE(m_pQuads);
    CC_SAFE_FREE(m_pIndices);
    
    glDeleteBuffers(2, m_pBuffersVBO);
    
    if (CAConfiguration::getInstance()->supportsShareableVAO())
    {
        glDeleteVertexArrays(1, &m_uVAOname);
        GL::bindVAO(0);
    }
    CC_SAFE_RELEASE(m_pImage);
    
}

unsigned int CAImageAtlas::getTotalQuads()
{
    return m_uTotalQuads;
}

unsigned int CAImageAtlas::getCapacity()
{
    return m_uCapacity;
}

CAImage* CAImageAtlas::getImage()
{
    return m_pImage;
}

void CAImageAtlas::setImage(CAImage * var)
{
    CC_SAFE_RETAIN(var);
    CC_SAFE_RELEASE(m_pImage);
    m_pImage = var;
}

ccV3F_C4B_T2F_Quad* CAImageAtlas::getQuads()
{
    //if someone accesses the quads directly, presume that changes will be made
    m_bDirty = true;
    return m_pQuads;
}

void CAImageAtlas::setQuads(ccV3F_C4B_T2F_Quad *var)
{
    m_pQuads = var;
}

CAImageAtlas * CAImageAtlas::createWithImage(CAImage *image, unsigned int capacity)
{
    CAImageAtlas * pImageAtlas = new CAImageAtlas();
    if (pImageAtlas && pImageAtlas->initWithImage(image, capacity))
    {
        pImageAtlas->autorelease();
        return pImageAtlas;
    }
    CC_SAFE_DELETE(pImageAtlas);
    return NULL;
}

bool CAImageAtlas::initWithImage(CAImage *image, unsigned int capacity)
{
    //    CCAssert(image != NULL, "image should not be null");
    m_uCapacity = capacity;
    m_uTotalQuads = 0;
    
    // retained in property
    this->m_pImage = image;
    CC_SAFE_RETAIN(m_pImage);
    
    // Re-initialization is not allowed
    CCAssert(m_pQuads == NULL && m_pIndices == NULL, "");
    
    m_pQuads = (ccV3F_C4B_T2F_Quad*)malloc( m_uCapacity * sizeof(ccV3F_C4B_T2F_Quad) );
    m_pIndices = (GLushort *)malloc( m_uCapacity * 6 * sizeof(GLushort) );
    
    if( ! ( m_pQuads && m_pIndices) && m_uCapacity > 0)
    {
        //CCLOG("CrossApp: CAImageAtlas: not enough memory");
        CC_SAFE_FREE(m_pQuads);
        CC_SAFE_FREE(m_pIndices);
        
        // release image, should set it to null, because the destruction will
        // release it too. see CrossApp issue #484
        CC_SAFE_RELEASE_NULL(m_pImage);
        return false;
    }
    
    memset( m_pQuads, 0, m_uCapacity * sizeof(ccV3F_C4B_T2F_Quad) );
    memset( m_pIndices, 0, m_uCapacity * 6 * sizeof(GLushort) );
    
#if CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID
    // listen the event when app go to background
    CANotificationCenter::getInstance()->addObserver(this,
                                                     callfuncO_selector(CAImageAtlas::listenBackToForeground),
                                                     EVENT_COME_TO_FOREGROUND,
                                                     NULL);
#endif
    
    this->setupIndices();
    
    if (CAConfiguration::getInstance()->supportsShareableVAO())
    {
        setupVBOandVAO();
    }
    else
    {
        setupVBO();
    }
    
    m_bDirty = true;
    
    return true;
}

void CAImageAtlas::listenBackToForeground(CAObject *obj)
{
    if (CAConfiguration::getInstance()->supportsShareableVAO())
    {
        setupVBOandVAO();
    }
    else
    {
        setupVBO();
    }
    
    // set m_bDirty to true to force it rebinding buffer
    m_bDirty = true;
}

const char* CAImageAtlas::description()
{
    const char* description;
    char tmp[128];
    sprintf(tmp, "<CAImageAtlas | totalQuads = %u>", m_uTotalQuads);
    description = tmp;
    return description;
}


void CAImageAtlas::setupIndices()
{
    if (m_uCapacity == 0)
        return;
    
    for( unsigned int i=0; i < m_uCapacity; i++)
    {
        m_pIndices[i*6+0] = i*4+0;
        m_pIndices[i*6+1] = i*4+1;
        m_pIndices[i*6+2] = i*4+2;
        
        // inverted index. issue #179
        m_pIndices[i*6+3] = i*4+3;
        m_pIndices[i*6+4] = i*4+2;
        m_pIndices[i*6+5] = i*4+1;
    }
}

//ImageAtlas - VAO / VBO specific

void CAImageAtlas::setupVBOandVAO()
{
    glGenVertexArrays(1, &m_uVAOname);
    GL::bindVAO(m_uVAOname);
    
#define kQuadSize sizeof(m_pQuads[0].bl)
    
    glGenBuffers(2, &m_pBuffersVBO[0]);
    
    glBindBuffer(GL_ARRAY_BUFFER, m_pBuffersVBO[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(m_pQuads[0]) * m_uCapacity, m_pQuads, GL_DYNAMIC_DRAW);
    
    // vertices
    glEnableVertexAttribArray(GLProgram::VERTEX_ATTRIB_POSITION);
    glVertexAttribPointer(GLProgram::VERTEX_ATTRIB_POSITION, 3, GL_FLOAT, GL_FALSE, kQuadSize, (GLvoid*) offsetof( ccV3F_C4B_T2F, vertices));
    
    // colors
    glEnableVertexAttribArray(GLProgram::VERTEX_ATTRIB_COLOR);
    glVertexAttribPointer(GLProgram::VERTEX_ATTRIB_COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE, kQuadSize, (GLvoid*) offsetof( ccV3F_C4B_T2F, colors));
    
    // tex coords
    glEnableVertexAttribArray(GLProgram::VERTEX_ATTRIB_TEX_COORD);
    glVertexAttribPointer(GLProgram::VERTEX_ATTRIB_TEX_COORD, 2, GL_FLOAT, GL_FALSE, kQuadSize, (GLvoid*) offsetof( ccV3F_C4B_T2F, texCoords));
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_pBuffersVBO[1]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(m_pIndices[0]) * m_uCapacity * 6, m_pIndices, GL_STATIC_DRAW);
    
    // Must unbind the VAO before changing the element buffer.
    GL::bindVAO(0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    
    CHECK_GL_ERROR_DEBUG();
}

void CAImageAtlas::setupVBO()
{
    glGenBuffers(2, &m_pBuffersVBO[0]);
    
    mapBuffers();
}

void CAImageAtlas::mapBuffers()
{
    // Avoid changing the element buffer for whatever VAO might be bound.
	GL::bindVAO(0);
    
    glBindBuffer(GL_ARRAY_BUFFER, m_pBuffersVBO[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(m_pQuads[0]) * m_uCapacity, m_pQuads, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_pBuffersVBO[1]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(m_pIndices[0]) * m_uCapacity * 6, m_pIndices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    
    CHECK_GL_ERROR_DEBUG();
}

// ImageAtlas - Update, Insert, Move & Remove

void CAImageAtlas::updateQuad(ccV3F_C4B_T2F_Quad *quad, unsigned int index)
{
    CCAssert( index >= 0 && index < m_uCapacity, "updateQuadWithImage: Invalid index");
    
    m_uTotalQuads = MAX( index+1, m_uTotalQuads);
    
    m_pQuads[index] = *quad;
    
    m_bDirty = true;
    
}

void CAImageAtlas::insertQuad(ccV3F_C4B_T2F_Quad *quad, unsigned int index)
{
    CCAssert( index < m_uCapacity, "insertQuadWithImage: Invalid index");
    
    m_uTotalQuads++;
    CCAssert( m_uTotalQuads <= m_uCapacity, "invalid totalQuads");
    
    // issue #575. index can be > totalQuads
    unsigned int remaining = (m_uTotalQuads-1) - index;
    
    // last object doesn't need to be moved
    if( remaining > 0)
    {
        // image coordinates
        memmove( &m_pQuads[index+1],&m_pQuads[index], sizeof(m_pQuads[0]) * remaining );
    }
    
    m_pQuads[index] = *quad;
    
    
    m_bDirty = true;
    
}

void CAImageAtlas::insertQuads(ccV3F_C4B_T2F_Quad* quads, unsigned int index, unsigned int amount)
{
    CCAssert(index + amount <= m_uCapacity, "insertQuadWithImage: Invalid index + amount");
    
    m_uTotalQuads += amount;
    
    CCAssert( m_uTotalQuads <= m_uCapacity, "invalid totalQuads");
    
    // issue #575. index can be > totalQuads
    int remaining = (m_uTotalQuads-1) - index - amount;
    
    // last object doesn't need to be moved
    if( remaining > 0)
    {
        // tex coordinates
        memmove( &m_pQuads[index+amount],&m_pQuads[index], sizeof(m_pQuads[0]) * remaining );
    }
    
    
    unsigned int max = index + amount;
    unsigned int j = 0;
    for (unsigned int i = index; i < max ; i++)
    {
        m_pQuads[index] = quads[j];
        index++;
        j++;
    }
    
    m_bDirty = true;
}

void CAImageAtlas::insertQuadFromIndex(unsigned int oldIndex, unsigned int newIndex)
{
    CCAssert( newIndex >= 0 && newIndex < m_uTotalQuads, "insertQuadFromIndex:atIndex: Invalid index");
    CCAssert( oldIndex >= 0 && oldIndex < m_uTotalQuads, "insertQuadFromIndex:atIndex: Invalid index");
    
    if( oldIndex == newIndex )
    {
        return;
    }
    // because it is ambiguous in iphone, so we implement abs ourselves
    // unsigned int howMany = abs( oldIndex - newIndex);
    unsigned int howMany = (oldIndex - newIndex) > 0 ? (oldIndex - newIndex) :  (newIndex - oldIndex);
    unsigned int dst = oldIndex;
    unsigned int src = oldIndex + 1;
    if( oldIndex > newIndex)
    {
        dst = newIndex+1;
        src = newIndex;
    }
    
    // image coordinates
    ccV3F_C4B_T2F_Quad quadsBackup = m_pQuads[oldIndex];
    memmove( &m_pQuads[dst],&m_pQuads[src], sizeof(m_pQuads[0]) * howMany );
    m_pQuads[newIndex] = quadsBackup;
    
    
    m_bDirty = true;
    
}

void CAImageAtlas::removeQuadAtIndex(unsigned int index)
{
    CCAssert( index < m_uTotalQuads, "removeQuadAtIndex: Invalid index");
    
    unsigned int remaining = (m_uTotalQuads-1) - index;
    
    
    // last object doesn't need to be moved
    if( remaining )
    {
        // image coordinates
        memmove( &m_pQuads[index],&m_pQuads[index+1], sizeof(m_pQuads[0]) * remaining );
    }
    
    m_uTotalQuads--;
    
    
    m_bDirty = true;
    
}

void CAImageAtlas::removeQuadsAtIndex(unsigned int index, unsigned int amount)
{
    CCAssert(index + amount <= m_uTotalQuads, "removeQuadAtIndex: index + amount out of bounds");
    
    unsigned int remaining = (m_uTotalQuads) - (index + amount);
    
    m_uTotalQuads -= amount;
    
    if ( remaining )
    {
        memmove( &m_pQuads[index], &m_pQuads[index+amount], sizeof(m_pQuads[0]) * remaining );
    }
    
    m_bDirty = true;
}

void CAImageAtlas::removeAllQuads()
{
    m_uTotalQuads = 0;
}

// ImageAtlas - Resize
bool CAImageAtlas::resizeCapacity(unsigned int newCapacity)
{
    if( newCapacity == m_uCapacity )
    {
        return true;
    }
    unsigned int uOldCapactiy = m_uCapacity;
    // update capacity and totolQuads
    m_uTotalQuads = MIN(m_uTotalQuads, newCapacity);
    m_uCapacity = newCapacity;
    
    ccV3F_C4B_T2F_Quad* tmpQuads = NULL;
    GLushort* tmpIndices = NULL;
    
    // when calling initWithImage(fileName, 0) on bada device, calloc(0, 1) will fail and return NULL,
    // so here must judge whether m_pQuads and m_pIndices is NULL.
    if (m_pQuads == NULL)
    {
        tmpQuads = (ccV3F_C4B_T2F_Quad*)malloc( m_uCapacity * sizeof(m_pQuads[0]) );
        if (tmpQuads != NULL)
        {
            memset(tmpQuads, 0, m_uCapacity * sizeof(m_pQuads[0]) );
        }
    }
    else
    {
        tmpQuads = (ccV3F_C4B_T2F_Quad*)realloc( m_pQuads, sizeof(m_pQuads[0]) * m_uCapacity );
        if (tmpQuads != NULL && m_uCapacity > uOldCapactiy)
        {
            memset(tmpQuads+uOldCapactiy, 0, (m_uCapacity - uOldCapactiy)*sizeof(m_pQuads[0]) );
        }
    }
    
    if (m_pIndices == NULL)
    {
        tmpIndices = (GLushort*)malloc( m_uCapacity * 6 * sizeof(m_pIndices[0]) );
        if (tmpIndices != NULL)
        {
            memset( tmpIndices, 0, m_uCapacity * 6 * sizeof(m_pIndices[0]) );
        }
        
    }
    else
    {
        tmpIndices = (GLushort*)realloc( m_pIndices, sizeof(m_pIndices[0]) * m_uCapacity * 6 );
        if (tmpIndices != NULL && m_uCapacity > uOldCapactiy)
        {
            memset( tmpIndices+uOldCapactiy, 0, (m_uCapacity-uOldCapactiy) * 6 * sizeof(m_pIndices[0]) );
        }
    }
    
    if( ! ( tmpQuads && tmpIndices) ) {
        CCLOG("CrossApp: CAImageAtlas: not enough memory");
        CC_SAFE_FREE(tmpQuads);
        CC_SAFE_FREE(tmpIndices);
        CC_SAFE_FREE(m_pQuads);
        CC_SAFE_FREE(m_pIndices);
        m_uCapacity = m_uTotalQuads = 0;
        return false;
    }
    
    m_pQuads = tmpQuads;
    m_pIndices = tmpIndices;
    
    
    setupIndices();
    mapBuffers();
    
    m_bDirty = true;
    
    return true;
}

void CAImageAtlas::increaseTotalQuadsWith(unsigned int amount)
{
    m_uTotalQuads += amount;
}

void CAImageAtlas::moveQuadsFromIndex(unsigned int oldIndex, unsigned int amount, unsigned int newIndex)
{
    CCAssert(newIndex + amount <= m_uTotalQuads, "insertQuadFromIndex:atIndex: Invalid index");
    CCAssert(oldIndex < m_uTotalQuads, "insertQuadFromIndex:atIndex: Invalid index");
    
    if( oldIndex == newIndex )
    {
        return;
    }
    //create buffer
    size_t quadSize = sizeof(ccV3F_C4B_T2F_Quad);
    ccV3F_C4B_T2F_Quad* tempQuads = (ccV3F_C4B_T2F_Quad*)malloc( quadSize * amount);
    memcpy( tempQuads, &m_pQuads[oldIndex], quadSize * amount );
    
    if (newIndex < oldIndex)
    {
        // move quads from newIndex to newIndex + amount to make room for buffer
        memmove( &m_pQuads[newIndex], &m_pQuads[newIndex+amount], (oldIndex-newIndex)*quadSize);
    }
    else
    {
        // move quads above back
        memmove( &m_pQuads[oldIndex], &m_pQuads[oldIndex+amount], (newIndex-oldIndex)*quadSize);
    }
    memcpy( &m_pQuads[newIndex], tempQuads, amount*quadSize);
    
    free(tempQuads);
    
    m_bDirty = true;
}

void CAImageAtlas::moveQuadsFromIndex(unsigned int index, unsigned int newIndex)
{
    CCAssert(newIndex + (m_uTotalQuads - index) <= m_uCapacity, "moveQuadsFromIndex move is out of bounds");
    
    memmove(m_pQuads + newIndex,m_pQuads + index, (m_uTotalQuads - index) * sizeof(m_pQuads[0]));
}

void CAImageAtlas::fillWithEmptyQuadsFromIndex(unsigned int index, unsigned int amount)
{
    ccV3F_C4B_T2F_Quad quad;
    memset(&quad, 0, sizeof(quad));
    
    unsigned int to = index + amount;
    for (unsigned int i = index ; i < to ; i++)
    {
        m_pQuads[i] = quad;
    }
}

// ImageAtlas - Drawing

void CAImageAtlas::drawQuads()
{
    this->drawNumberOfQuads(m_uTotalQuads, 0);
}

void CAImageAtlas::drawNumberOfQuads(unsigned int n)
{
    this->drawNumberOfQuads(n, 0);
}

void CAImageAtlas::drawNumberOfQuads(unsigned int n, unsigned int start)
{
    if (0 == n)
    {
        return;
    }
    GL::bindTexture2D(m_pImage->getName());
    
    auto conf = CAConfiguration::getInstance();
    if (conf->supportsShareableVAO() && conf->supportsMapBuffer())
    {
        if (m_bDirty)
        {
            
            glBindBuffer(GL_ARRAY_BUFFER, m_pBuffersVBO[0]);
            
            glBufferData(GL_ARRAY_BUFFER, sizeof(m_pQuads[0]) * m_uCapacity, nullptr, GL_DYNAMIC_DRAW);
            void *buf = glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
            memcpy(buf, m_pQuads, sizeof(m_pQuads[0])* m_uTotalQuads);
            glUnmapBuffer(GL_ARRAY_BUFFER);
            
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            
            m_bDirty = false;
        }
        
        GL::bindVAO(m_uVAOname);
        
#if CC_REBIND_INDICES_BUFFER
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_pBuffersVBO[1]);
#endif
        
        glDrawElements(GL_TRIANGLES, (GLsizei) n*6, GL_UNSIGNED_SHORT, (GLvoid*) (start*6*sizeof(m_pIndices[0])) );
        
        GL::bindVAO(0);
        
#if CC_REBIND_INDICES_BUFFER
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
#endif
        
        //    glBindVertexArray(0);
    
    }
    else
    {// ! CC_TEXTURE_ATLAS_USE_VAO
    
        //
        // Using VBO without VAO
        //
        
#define kQuadSize sizeof(m_pQuads[0].bl)
        glBindBuffer(GL_ARRAY_BUFFER, m_pBuffersVBO[0]);
        
        // XXX: update is done in draw... perhaps it should be done in a timer
        if (m_bDirty)
        {
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(m_pQuads[0]) * m_uTotalQuads , &m_pQuads[0] );
            
            m_bDirty = false;
        }
        
        GL::enableVertexAttribs(GL::VERTEX_ATTRIB_FLAG_POS_COLOR_TEX);
        
        // vertices
        glVertexAttribPointer(GLProgram::VERTEX_ATTRIB_POSITION, 3, GL_FLOAT, GL_FALSE, kQuadSize, (GLvoid*) offsetof(ccV3F_C4B_T2F, vertices));
        
        // colors
        glVertexAttribPointer(GLProgram::VERTEX_ATTRIB_COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE, kQuadSize, (GLvoid*) offsetof(ccV3F_C4B_T2F, colors));
        
        // tex coords
        glVertexAttribPointer(GLProgram::VERTEX_ATTRIB_TEX_COORD, 2, GL_FLOAT, GL_FALSE, kQuadSize, (GLvoid*) offsetof(ccV3F_C4B_T2F, texCoords));
        
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_pBuffersVBO[1]);
        
        glDrawElements(GL_TRIANGLES, (GLsizei)n*6, GL_UNSIGNED_SHORT, (GLvoid*) (start*6*sizeof(m_pIndices[0])));
        
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }
    
    INCREMENT_GL_DRAWN_BATCHES_AND_VERTICES(1,n*6);
    CHECK_GL_ERROR_DEBUG();
}

NS_CC_END
