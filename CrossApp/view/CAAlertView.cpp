
#include "CAAlertView.h"
#include "platform/CAAlertViewImpl.h"
NS_CC_BEGIN

static CAMap<unsigned int, CAAlertView*> s_gMessageBoxs;

CAAlertView* CAAlertView::create(const char* title, const char* message)
{
    CAAlertView* box = new CAAlertView(title, message);
    box->m_vButtonTitles.push_back("OK");
    box->autorelease();
    return box;
}

CAAlertView* CAAlertView::create(const char* title, const char* message, const char* buttonTitle, ...)
{
    CAAlertView* box = new CAAlertView(title, message);
    {
        va_list args;
        va_start(args, buttonTitle);
        while (buttonTitle)
        {
            box->m_vButtonTitles.push_back(buttonTitle);
            buttonTitle = va_arg(args, const char*);
        }
        va_end(args);
    }
    box->autorelease();
    return box;
}

CAAlertView* CAAlertView::create(const char* title, const char* message, const std::vector<std::string>& buttonTitles)
{
    CAAlertView* box = new CAAlertView(title, message);
    box->m_vButtonTitles = buttonTitles;
    box->autorelease();
    return box;
}

CAAlertView::CAAlertView(const char* title, const char* message)
:m_sTitle(title)
,m_sMessage(message)
,m_callback(nullptr)
{
    
}


void CAAlertView::show()
{
    s_gMessageBoxs.insert(m_u__ID, this);
    __show_alertView(m_sTitle, m_sMessage, m_vButtonTitles, [&](int index)
    {
        s_gMessageBoxs.erase(m_u__ID);
    });
    
}
void CAAlertView::show(const std::function<void(int)>& callback)
{
    m_callback = callback;
    s_gMessageBoxs.insert(m_u__ID, this);
    __show_alertView(m_sTitle, m_sMessage, m_vButtonTitles, [&](int index)
    {
        if (m_callback)
        {
            m_callback(index);
        }
        s_gMessageBoxs.erase(m_u__ID);
    });
    
}

NS_CC_END
