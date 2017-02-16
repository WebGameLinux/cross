//
//  CAScale9ImageView.h
//  CrossApp
//
//  Created by Li Yuanfeng on 14-6-17.
//  Copyright (c) 2014 http://9miao.com All rights reserved.
//

#include "CAScale9ImageView.h"
#include "animation/CAViewAnimation.h"

NS_CC_BEGIN

CAScale9ImageView::CAScale9ImageView()
: m_fInsetLeft(0)
, m_fInsetTop(0)
, m_fInsetRight(0)
, m_fInsetBottom(0)
, m_obCapInsets(DRectZero)
, m_obOriginalSize(DSizeZero)
, m_pScale9ImageView(nullptr)
, m_bUpdatePositions(false)
, m_bAutoCapInsets(true)
{
    memset(m_pImageView, 0, sizeof(m_pImageView));
}

CAScale9ImageView::~CAScale9ImageView()
{

}

CAScale9ImageView* CAScale9ImageView::create()
{
    CAScale9ImageView *pReturn = new CAScale9ImageView();
    if (pReturn && pReturn->init())
    {
        pReturn->autorelease();
        return pReturn;
    }
    CC_SAFE_DELETE(pReturn);
    return NULL;
}

CAScale9ImageView* CAScale9ImageView::createWithImage(CAImage* image)
{
	CAScale9ImageView* pReturn = new CAScale9ImageView();
	if (pReturn && pReturn->initWithImage(image))
	{
		pReturn->autorelease();
		return pReturn;
	}
	CC_SAFE_DELETE(pReturn);
	return NULL;
}

CAScale9ImageView* CAScale9ImageView::createWithFrame(const DRect& rect)
{
	CAScale9ImageView* pReturn = new CAScale9ImageView();
	if (pReturn && pReturn->initWithFrame(rect))
	{
		pReturn->autorelease();
		return pReturn;
	}
	CC_SAFE_DELETE(pReturn);
	return NULL;
}

CAScale9ImageView* CAScale9ImageView::createWithCenter(const DRect& rect)
{
    CAScale9ImageView* pReturn = new CAScale9ImageView();
	if (pReturn && pReturn->initWithCenter(rect))
	{
		pReturn->autorelease();
		return pReturn;
	}
	CC_SAFE_DELETE(pReturn);
	return NULL;
}

CAScale9ImageView* CAScale9ImageView::createWithLayout(const CrossApp::DLayout &layout)
{
    CAScale9ImageView* pReturn = new CAScale9ImageView();
    if (pReturn && pReturn->initWithLayout(layout))
    {
        pReturn->autorelease();
        return pReturn;
    }
    CC_SAFE_DELETE(pReturn);
    return NULL;
}

bool CAScale9ImageView::init()
{
    return this->initWithImage(nullptr);
}

bool CAScale9ImageView::initWithImage(CAImage* image)
{
    if (image)
    {
        this->setImage(image);
    }
    
	return true;
}

CAView* CAScale9ImageView::copy()
{
    CAScale9ImageView* pReturn = CAScale9ImageView::createWithImage(this->getImage());
    pReturn->setColor(this->getColor());
    if (m_eLayoutType == 0)
    {
        pReturn->setFrame(this->getFrame());
    }
    else if (m_eLayoutType == 1)
    {
        pReturn->setCenter(this->getCenter());
    }
    else
    {
        pReturn->setLayout(this->getLayout());
    }
    return pReturn;
}


void CAScale9ImageView::updateWithImage()
{
    CAColor4B color = this->getColor();

    if (m_pScale9ImageView)
    {
        m_pScale9ImageView->removeAllSubviews();
        m_pScale9ImageView->removeFromSuperview();
        m_pScale9ImageView = NULL;
    }
    
    memset(m_pImageView, 0, sizeof(m_pImageView));

    CC_RETURN_IF(!m_pobImage);

    m_pScale9ImageView = new CAView();
    m_pScale9ImageView->setLayout(DLayoutFill);
    this->addSubview(m_pScale9ImageView);
    m_pScale9ImageView->release();
    
    m_obOriginalSize = m_pobImage->getContentSize();
    
    for (int i=0; i<9; i++)
    {
        m_pImageView[i] = CAImageView::createWithImage(m_pobImage);
    }
    
    // Centre
    m_pScale9ImageView->insertSubview(m_pImageView[4], 0);
    
    // Top
    m_pScale9ImageView->insertSubview(m_pImageView[1], 0);
    
    // Bottom
    m_pScale9ImageView->insertSubview(m_pImageView[7], 0);
    
    // Left
    m_pScale9ImageView->insertSubview(m_pImageView[3], 0);
    
    // Right
    m_pScale9ImageView->insertSubview(m_pImageView[5], 0);
    
    // Top left
    m_pScale9ImageView->insertSubview(m_pImageView[0], 0);
    
    // Top right
    m_pScale9ImageView->insertSubview(m_pImageView[2], 0);
    
    // Bottom left
    m_pScale9ImageView->insertSubview(m_pImageView[6], 0);
    
    // Bottom right
    m_pScale9ImageView->insertSubview(m_pImageView[8], 0);
    
    this->updateCapInset();
    
    this->setColor(color); 
}

void CAScale9ImageView::updatePositions()
{
    CC_RETURN_IF(m_pobImage == NULL);
    CC_RETURN_IF(m_bUpdatePositions);
    m_bUpdatePositions = true;
    
    DSize size = this->m_obContentSize;
    
    DRect newRect[9];
    
    for (int i=0; i<9; i++)
    {
        newRect[i] = m_rFrame[i];
    }
    
    if (size.width > m_obOriginalSize.width)
    {
        const float lenghtX1 = m_rFrame[0].size.width;
        const float lenghtX3 = m_rFrame[2].size.width;
        const float lenghtX2 = size.width - lenghtX1 - lenghtX3;
        
        newRect[1].origin.x = lenghtX1;
        newRect[4].origin.x = lenghtX1;
        newRect[7].origin.x = lenghtX1;
        
        newRect[1].size.width = lenghtX2;
        newRect[4].size.width = lenghtX2;
        newRect[7].size.width = lenghtX2;
        
        newRect[2].origin.x = lenghtX1 + lenghtX2;
        newRect[5].origin.x = lenghtX1 + lenghtX2;
        newRect[8].origin.x = lenghtX1 + lenghtX2;
        
        newRect[2].size.width = lenghtX3;
        newRect[5].size.width = lenghtX3;
        newRect[8].size.width = lenghtX3;
    }
    else
    {
        float scaleX = size.width / m_obOriginalSize.width;
        for (int i=0; i<9; i++)
        {
            newRect[i].origin.x *= scaleX;
            newRect[i].size.width *= scaleX;
        }
    }
    
    if (size.height > m_obOriginalSize.height)
    {
        const float lenghtY1 = m_rFrame[0].size.height;
        const float lenghtY3 = m_rFrame[6].size.height;
        const float lenghtY2 = size.height - lenghtY1 - lenghtY3;
        
        newRect[3].origin.y = lenghtY1;
        newRect[4].origin.y = lenghtY1;
        newRect[5].origin.y = lenghtY1;
        
        newRect[3].size.height = lenghtY2;
        newRect[4].size.height = lenghtY2;
        newRect[5].size.height = lenghtY2;
        
        newRect[6].origin.y = lenghtY1 + lenghtY2;
        newRect[7].origin.y = lenghtY1 + lenghtY2;
        newRect[8].origin.y = lenghtY1 + lenghtY2;
        
        newRect[6].size.height = lenghtY3;
        newRect[7].size.height = lenghtY3;
        newRect[8].size.height = lenghtY3;
    }
    else
    {
        float scaleY = size.height / m_obOriginalSize.height;
        for (int i=0; i<9; i++)
        {
            newRect[i].origin.y *= scaleY;
            newRect[i].size.height *= scaleY;
        }
    }
    
    for (int i=0; i<9; i++)
    {
        m_pImageView[i]->setFrame(newRect[i]);
    }
    
    m_bUpdatePositions = false;
}

void CAScale9ImageView::setContentSize(const DSize &size)
{
    CAView::setContentSize(size);
    
    this->updateCapInset();
}

void CAScale9ImageView::draw(Renderer *renderer, const Mat4& transform, uint32_t flags)
{

}

void CAScale9ImageView::setCapInsets(const DRect& capInsets)
{
    CC_RETURN_IF(m_obCapInsets.equals(capInsets));
    m_obCapInsets = capInsets;
    m_bAutoCapInsets = false;
    this->updateCapInset();
}

void CAScale9ImageView::updateCapInset()
{
    float w = m_obOriginalSize.width;
    float h = m_obOriginalSize.height;
    
    if (m_bAutoCapInsets)
    {
        m_obCapInsets = DRect(w/3, h/3, w/3, h/3);
    }
    
    const float lenghtX1 = m_obCapInsets.origin.x;
    const float lenghtX2 = m_obCapInsets.size.width;
    const float lenghtX3 = w - lenghtX1 - lenghtX2;
    const float lenghtY1 = m_obCapInsets.origin.y;
    const float lenghtY2 = m_obCapInsets.size.height;
    const float lenghtY3 = h - lenghtY1 - lenghtY2;
    
    m_rFrame[0] = DRect(0, 0, lenghtX1, lenghtY1);
    m_rFrame[1] = DRect(lenghtX1, 0, lenghtX2, lenghtY1);
    m_rFrame[2] = DRect(lenghtX1 + lenghtX2, 0, lenghtX3, lenghtY1);
    m_rFrame[3] = DRect(0, lenghtY1, lenghtX1, lenghtY2);
    m_rFrame[4] = DRect(lenghtX1, lenghtY1, lenghtX2, lenghtY2);
    m_rFrame[5] = DRect(lenghtX1 + lenghtX2, lenghtY1, lenghtX3, lenghtY2);
    m_rFrame[6] = DRect(0, lenghtY1 + lenghtY2, lenghtX1, lenghtY3);
    m_rFrame[7] = DRect(lenghtX1, lenghtY1 + lenghtY2, lenghtX2, lenghtY3);
    m_rFrame[8] = DRect(lenghtX1 + lenghtX2, lenghtY1 + lenghtY2, lenghtX3, lenghtY3);
    
    CC_RETURN_IF(m_pobImage == NULL);
    
    if (m_pScale9ImageView)
    {
        for (int i=0; i<9; i++)
        {
            m_pImageView[i]->setImageRect(m_rFrame[i]);
        }
        this->updatePositions();
    }
}

void CAScale9ImageView::updateDisplayedColor(const CrossApp::CAColor4B &color)
{
    CAView::updateDisplayedColor(color);
    setColor(color);
}

void CAScale9ImageView::setInsetLeft(float insetLeft)
{
    this->m_fInsetLeft = insetLeft;
    m_bAutoCapInsets = false;
    DRect insets = m_obCapInsets;
    if (m_fInsetLeft != 0 && m_fInsetTop != 0 && m_fInsetRight != 0 && m_fInsetBottom != 0)
    {
        insets = DRect(m_fInsetLeft,
                        m_fInsetTop,
                        m_obOriginalSize.width-m_fInsetLeft-m_fInsetRight,
                        m_obOriginalSize.height-m_fInsetTop-m_fInsetBottom);
        this->updateCapInset();
    }
}

void CAScale9ImageView::setInsetTop(float insetTop)
{
    this->m_fInsetTop = insetTop;
    m_bAutoCapInsets = false;
    DRect insets = m_obCapInsets;
    if (m_fInsetLeft != 0 && m_fInsetTop != 0 && m_fInsetRight != 0 && m_fInsetBottom != 0)
    {
        insets = DRect(m_fInsetLeft,
                        m_fInsetTop,
                        m_obOriginalSize.width-m_fInsetLeft-m_fInsetRight,
                        m_obOriginalSize.height-m_fInsetTop-m_fInsetBottom);
        this->updateCapInset();
    }
}

void CAScale9ImageView::setInsetRight(float insetRight)
{
    this->m_fInsetRight = insetRight;
    m_bAutoCapInsets = false;
    DRect insets = m_obCapInsets;
    if (m_fInsetLeft != 0 && m_fInsetTop != 0 && m_fInsetRight != 0 && m_fInsetBottom != 0)
    {
        insets = DRect(m_fInsetLeft,
                        m_fInsetTop,
                        m_obOriginalSize.width-m_fInsetLeft-m_fInsetRight,
                        m_obOriginalSize.height-m_fInsetTop-m_fInsetBottom);
        this->updateCapInset();
    }
}

void CAScale9ImageView::setInsetBottom(float insetBottom)
{
    this->m_fInsetBottom = insetBottom;
    m_bAutoCapInsets = false;
    DRect insets = m_obCapInsets;
    if (m_fInsetLeft != 0 && m_fInsetTop != 0 && m_fInsetRight != 0 && m_fInsetBottom != 0)
    {
        insets = DRect(m_fInsetLeft,
                        m_fInsetTop,
                        m_obOriginalSize.width-m_fInsetLeft-m_fInsetRight,
                        m_obOriginalSize.height-m_fInsetTop-m_fInsetBottom);
        this->updateCapInset();
    }
}

void CAScale9ImageView::setColor(const CAColor4B& color)
{
    CAView::setColor(color);
    if (m_pScale9ImageView)
    {
        for (auto& subview : m_pScale9ImageView->getSubviews())
        {
            subview->setColor(color);
        }
    }
}

const CAColor4B& CAScale9ImageView::getColor()
{
	return CAView::getColor();
}

void CAScale9ImageView::setAlpha(float alpha)
{
    CAView::setAlpha(alpha);
}

void CAScale9ImageView::setImage(CrossApp::CAImage *image)
{
    CAView::setImage(image);
    this->updateWithImage();
}


NS_CC_END