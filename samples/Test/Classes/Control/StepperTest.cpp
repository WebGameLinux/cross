
#include "StepperTest.h"

StepperTest::StepperTest()
{
    CADrawerController* drawer = (CADrawerController*)CAApplication::getApplication()->getRootWindow()->getRootViewController();
    drawer->setTouchMoved(false);
}

StepperTest::~StepperTest()
{
    CADrawerController* drawer = (CADrawerController*)CAApplication::getApplication()->getRootWindow()->getRootViewController();
    drawer->setTouchMoved(true);
}

void StepperTest::viewDidLoad()
{
    this->getView()->setColor(CAColor_gray);
    
    step_value = CALabel::createWithLayout(DLayout(DHorizontalLayout_W_C(300, 0.5), DVerticalLayout_H_C(100, 0.25)));
    step_value->setText("step_value:0");
    step_value->setColor(CAColor_black);
    step_value->setFontSize(28);
    step_value->setTextAlignment(CATextAlignment::Center);
    step_value->setVerticalTextAlignmet(CAVerticalTextAlignment::Center);
    this->getView()->addSubview(step_value);
    
    step = CAStepper::createWithLayout(DLayout(DHorizontalLayout_W_C(360, 0.5), DVerticalLayout_H_C(60, 0.5)), CAStepper::Orientation::Horizontal);
    //step->setWraps(true);//是否循环,默认循环
    step->setMinValue(0);
    step->setMaxValue(50);
    step->setStepValue(1);
    //step->setAutoRepeat(false);
    this->getView()->addSubview(step);
    step->setTarget([=](CAStepper* stepper, int index)
    {
        char tem[30];
        sprintf(tem, "step_value:%.0f",step->getValue());
        step_value->setText(tem);
        CCLog("step-tag === %f",step->getValue());
    });
}

void StepperTest::viewDidUnload()
{
    // Release any retained subviews of the main view.
    // e.g. self.myOutlet = nil;
}


