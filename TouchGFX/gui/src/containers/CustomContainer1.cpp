#include <gui/containers/CustomContainer1.hpp>
#include <texts/TextKeysAndLanguages.hpp>
#include <touchgfx/Color.hpp>

CustomContainer1::CustomContainer1()
{

}

void CustomContainer1::initialize()
{
    CustomContainer1Base::initialize();
}

void CustomContainer1::setRecipeName(const char* name)
{
    Unicode::strncpy(textArea1Buffer, name, TEXTAREA1_SIZE);
    textArea1.invalidate();
}

void CustomContainer1::setSelected(bool isSelected)
{
    if (isSelected)
    {
        box1.setColor(touchgfx::Color::getColorFromRGB(0, 160, 230));
    }
    else
    {
        box1.setColor(touchgfx::Color::getColorFromRGB(120, 80, 80));
    }
    box1.invalidate(); 
}