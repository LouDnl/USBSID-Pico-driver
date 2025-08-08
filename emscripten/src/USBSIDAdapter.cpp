// #ifdef EMSCRIPTEN
#include <emscripten.h>
#include <emscripten/bind.h>
#include <emscripten/val.h>

#include "USBSID.h"
using USBSID_NS::USBSID_Class;
using namespace USBSID_NS;

// using namespace emscripten;
// using namespace std;

// typedef void * USBSIDitf;
/* static USBSID_Class* usbsid; */

// USBSID_Class* passThrough(USBSID_Class* ptr) { return ptr; }
// USBSID_Class* createC() { return new USBSID_Class(); }

// extern "C"
// {
//   // typedef USBSID_Class USBSIDitf;

//   void *create_USBSID(void){
//     return (USBSID_Class*) new USBSID_Class();
//   };
//   USBSID_Class* passThrough(USBSID_Class* ptr) { return ptr; }
  // int init_USBSID(USBSIDitf p, bool start_threaded, bool with_cycles){
  //   if( p == NULL ) return -1;
  //   return ((USBSID_Class*) p)->USBSID_Init(start_threaded, with_cycles);
  // };
  // void close_USBSID(USBSIDitf p){
  //   if( p == NULL ) return;
  //   delete (USBSID_Class*) p;
  // };
// }

// EMBIND(usbsid, create_USBSID()) { return new USBSID_Class(); }
// EMBIND(int, init_USBSID()) { return usbsid::USBSID_Init(true, true); }

EMSCRIPTEN_BINDINGS(us_class)
{
  emscripten::class_<USBSID_Class>("USBSID_Class")
    .constructor<>()
    .function("USBSID_Init", &USBSID_Class::USBSID_Init)
    .function("USBSID_Close", &USBSID_Class::USBSID_Close)
    // .function("USBSID_GetSocketConfig", &USBSID_Class::USBSID_GetSocketConfig)
    .function("USBSID_SetClockRate", &USBSID_Class::USBSID_SetClockRate)
    .function("USBSID_GetClockRate", &USBSID_Class::USBSID_GetClockRate)
    .function("USBSID_GetRefreshRate", &USBSID_Class::USBSID_GetRefreshRate)
    .function("USBSID_GetRasterRate", &USBSID_Class::USBSID_GetRasterRate)
    ;
}

// EMSCRIPTEN_BINDINGS(raw_pointers)
// {
//   // emscripten::class_<USBSID_Class>("USBSID_Class")
//     function("create_USBSID", &create_USBSID, return_value_policy::reference())
//     function("passThrough", &passThrough, emscripten::allow_raw_pointers())
//   ;
// }

// // Binding code =======
// EXTERN EMSCRIPTEN_BINDINGS(pissoff) {
//   class_<USBSID_Class>("USBSID_Class")
//     .constructor<>()
//     .function("USBSID_Init", &usbsid::USBSID_Init)
//     .function("USBSID_Close", &usbsid::USBSID_Close)
//     ;
// }
  // .class_function("getStringFromInstance", &MyClass::getStringFromInstance)
    // .property("x_readonly", &MyClass::getX)
    // .property("x", &MyClass::getX, &MyClass::setX)
/*
navigator.usb.requestDevice({ filters: [] });
var us = new backend_SID.Module._create_USBSID();
backend_SID.Module._init_USBSID();
new backend_SID.Module.ccall('USBSID_Class','object',[],[])
backend_SID.Module.ccall('init_USBSID','number',['boolean','boolean'],[true,true])
 */
