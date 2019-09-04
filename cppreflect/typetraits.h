#pragma once
#include <vector>
#include <string>                       //std::vector
#include "enumreflect.h"                //EnumToString
#include "pugixml/pugixml.hpp"          //as_wide, as_utf8
#ifndef _MSC_VER
#include <cxxabi.h>                     //__cxa_demangle
#endif

class CppTypeInfo;
class ReflectClass;

//
//  Base class for performing field conversion to string / from string.
//
class TypeTraits
{
public:
    //
    //  Returns true if field type is primitive (int, string, etc...) - so all types which are not complex.
    //  Complex class type is derived from ReflectClass - so ReflectClassPtr() & GetType() returns non-null
    //
    virtual bool IsPrimitiveType()
    {
        return true;
    }

    virtual std::string name()
    {
        return std::string();
    }

    //
    //  Gets field complex type()
    //
    virtual CppTypeInfo* GetFieldType()
    {
        return nullptr;
    }

    //
    // Converts instance pointers to ReflectClass*.
    //
    virtual ReflectClass* ReflectClassPtr( void* p )
    {
        return nullptr;
    }

    virtual bool GetArrayElementType( CppTypeInfo*& type )
    {
        // Not array type
        return false;
    }

    //
    // If GetArrayElementType() returns true, returns size of array.
    //
    virtual size_t ArraySize( void* p )
    {
        return 0;
    }

    virtual void SetArraySize( void* p, size_t size )
    {
    }

    //
    //  Gets field (at p) array element at position i.
    //
    virtual void* ArrayElement( void* p, size_t i )
    {
        return nullptr;     // Invalid operation, since not array
    }

    //
    // Converts specific data to String.
    //
    // Default implementation: Don't know how to print given field, ignore it
    //
    virtual std::wstring ToString( void* pField ) { return std::wstring(); }
    
    //
    // Converts from String to data.
    //
    // Default implementation: Value cannot be set from string.
    //
    virtual void FromString( void* pField, const wchar_t* value ) { }
};



template <class T>
std::string getTypeName()
{
    typedef typename std::remove_reference<T>::type TR;
    std::unique_ptr<char, void(*)(void*)> own
    (
#ifndef _MSC_VER
        abi::__cxa_demangle(typeid(TR).name(), nullptr, nullptr, nullptr),
#else
        nullptr,
#endif
        std::free
    );
    std::string r = own != nullptr ? own.get() : typeid(TR).name();

#ifdef _MSC_VER
    if (strncmp(r.c_str(), "class ", 6) == 0)
        r = r.substr(6);
#endif

    return r;
}

// SFINAE test
template <typename T>
class HasGetType
{
private:
    typedef char YesType[1];
    typedef char NoType[2];

    template <typename C> static YesType& test(decltype(&C::GetType));
    template <typename C> static NoType& test(...);


public:
    enum { value = sizeof(test<T>(0)) == sizeof(YesType) };
};

template<typename T>
typename std::enable_if<HasGetType<T>::value, CppTypeInfo*>::type CallToGetType(CppTypeInfo*& type)
{
    return type = &T::GetType();
}

template<typename T>
CppTypeInfo* CallToGetType(...)
{
    return nullptr;
}



//
//  Generic class definition, which can be applied to any class type. This implementation however does nothing -
//  does not serializes or deserializes your type. You must override with your type specific implementation
//  for each supported data type. For more examples - see below.
//
template <class T>
class TypeTraitsT : public TypeTraits
{
public:
    virtual bool IsPrimitiveType()
    {
        return ! std::is_base_of<ReflectClass, T>::value;
    }

    virtual std::string name()
    {
        return getTypeName<T>();
    }

    CppTypeInfo* GetFieldType()
    {
        CppTypeInfo* tinfo = nullptr;
        return CallToGetType<T>(tinfo);
    }

    virtual ReflectClass* ReflectClassPtr( void* p )
    {
        if constexpr (std::is_base_of<ReflectClass, T>::value )
            // Works without dynamic_cast, compiler does not likes dynamic_cast in here.
            return (ReflectClass*)(T*)p;
        else
            return nullptr;
    }

    virtual std::wstring ToString( void* p )
    { 
        if constexpr ( std::is_enum<T>::value )
            return pugi::as_wide(EnumToString( *((T*)p) )).c_str();
        else
            return std::wstring(); 
    }

    virtual void FromString(void* p, const wchar_t* value)
    {
        if constexpr (std::is_enum<T>::value)
            StringToEnum(pugi::as_utf8(value).c_str(), *((T*)p));
    }
};

template <>
class TypeTraitsT<std::wstring> : public TypeTraits
{
public:
    virtual std::string name() { return "std::wstring"; }

    virtual std::wstring ToString(void* pField)
    {
        return ((std::wstring*)pField)->c_str();
    }

    virtual void FromString(void* pField, const wchar_t* value)
    {
        *((std::wstring*)pField) = value;
    }
};


template <>
class TypeTraitsT<std::string> : public TypeTraits
{
public:
    virtual std::string name() { return "std::string"; }

    virtual std::wstring ToString(void* pField)
    {
        return pugi::as_wide(((std::string*)pField)->c_str());
    }

    virtual void FromString(void* pField, const wchar_t* value)
    {
        auto& s = *((std::string*)pField);
        s = pugi::as_utf8(value);
    }
};


template <>
class TypeTraitsT<int> : public TypeTraits
{
public:
    virtual std::string name() { return "int"; }

    virtual std::wstring ToString( void* pField )
    {
        int* p = (int*) pField;
        char buf[10];
        #ifdef _WIN32
        _itoa_s(*p, buf, 10); 
        #else
        snprintf(buf, sizeof(buf), "%d", *p);
        #endif
        return pugi::as_wide(buf);
    }

    virtual void FromString( void* pField, const wchar_t* value )
    {
        int* p = (int*)pField;
        #ifdef _WIN32
        *p = _wtoi(value);
        #else
        wchar_t* pEnd;
        *p = wcstol(value, &pEnd, 10);
        #endif
    }
};

template <>
class TypeTraitsT<bool> : public TypeTraits
{
public:
    virtual std::string name() { return "bool"; }

    virtual std::wstring ToString( void* p )
    {
        if( *(bool*)p )
            return L"true";

        return L"false";
    }
    
    virtual void FromString( void* pField, const wchar_t* value )
    {
        bool* pb = (bool*)pField;
    #ifdef _WIN32
        if( _wcsicmp(value, L"true") == 0 )
    #else
        if (wcscasecmp(value, L"true") == 0)
    #endif
        {
            *pb = true;
            return;
        }

        *pb = false;
    }
};

// Just an example how primitive data types could be redefined. Serialize bool as "True/False" instead of "true/false".
class CamelCaseBool
{
public:
    bool value;
};

template <>
class TypeTraitsT<CamelCaseBool> : public TypeTraitsT<bool>
{
public:
    virtual std::string name() { return "CamelCaseBool"; }

    virtual std::wstring ToString(void* p)
    {
        if (((CamelCaseBool*)p)->value)
            return L"True";

        return L"False";
    }

    virtual void FromString(void* pField, const wchar_t* value)
    {
        TypeTraitsT<bool>::FromString(&((CamelCaseBool*)pField)->value, value);
    }
};


template <class E>
class TypeTraitsT< std::vector<E> > : public TypeTraits
{
public:
    virtual std::string name() { return getTypeName<std::vector<E>>(); }
    
    virtual bool GetArrayElementType( CppTypeInfo*& type )
    {
        type = CallToGetType<E>(type);
        return type != nullptr;
    }
    
    virtual size_t ArraySize( void* p )
    {
        std::vector<E>* v = (std::vector<E>*) p;
        return v->size();
    }

    virtual void SetArraySize( void* p, size_t size )
    {
        std::vector<E>* v = (std::vector<E>*) p;
        v->resize(size);
    }

    virtual void* ArrayElement( void* p, size_t i )
    {
        std::vector<E>* v = (std::vector<E>*) p;
        return &v->at( i );
    }

    virtual std::wstring ToString( void* pField )
    {
        return std::wstring();
    }
};

