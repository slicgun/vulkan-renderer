#include "Application.h"

#include<iostream>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

int main()
{
    Application app;

    app.update();
    return 0;
}

//todo:
//refactor
//move swap chain into class
//move pipeline into class
//make render commands not poop
//clean up all code
//make it more opengly if possible
//...
//figure out render commands

//--- info ---
//plan is to make a renderer which supports opengl and vulkan
//naming is very verbose just so i can seperate vulkan/opengl stuff very serperately
//so atm you will have Renderer::Vulkan::xxx where xxx = buffer/image/texture etc...
//same thing with opengl, Renderer::OpenGL::xxx

//idea is that i can should be able to take this code to other projects without much hastle -
//i am aware of the fact i could use the preprocessor to seperate this stuff, i just think -
//this will result in code which is easier to learn from/with