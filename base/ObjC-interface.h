#ifndef ObjC_interface_h
#define ObjC_interface_h

extern "C"
{
    void *getViewFromNSWindowPointer(void *NSWindow);
    void makeViewMetalCompatible(void *handle);
}

#endif