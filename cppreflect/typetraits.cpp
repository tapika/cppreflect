#include "cppreflect.h"

//
// http://www.cplusplus.com/reference/string/to_string/
//
template <>
const wchar_t* BasicStdTypeInfoT<int64_t>::fmt = L"%ld";

template <>
const char* BasicStdTypeInfoT<int64_t>::typeName = "int64";


