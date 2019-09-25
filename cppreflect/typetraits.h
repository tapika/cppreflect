#pragma once
#include <vector>
#include <string>                       //std::vector
#include "enumreflect.h"                //EnumToString
#include "pugixml/pugixml.hpp"          //as_wide, as_utf8
#ifndef _MSC_VER
#include <cxxabi.h>                     //__cxa_demangle
#endif

class ClassTypeInfo;
class ReflectClass;


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
typename std::enable_if<HasGetType<T>::value, ClassTypeInfo*>::type CallToGetType(ClassTypeInfo*& type)
{
    return type = &T::GetType();
}

template<typename T>
ClassTypeInfo* CallToGetType(...)
{
    return nullptr;
}



//
//  Generic class definition, which can be applied to any class type. This implementation however does nothing -
//  does not serializes or deserializes your type. You must override with your type specific implementation
//  for each supported data type. For more examples - see below.
//
template <class T>
class BasicTypeInfoT : public BasicTypeInfo
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

    BasicTypeInfo* GetClassType()
    {
        ClassTypeInfo* tinfo = nullptr;
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

    virtual size_t GetRawSize(void* pField)
    {
        return sizeof(T);
    }

    virtual size_t GetFixedSize()
    {
        return sizeof(T);
    }

    virtual size_t GetSizeOfType()
    {
        return sizeof(T);
    }
};

template <>
class BasicTypeInfoT<std::wstring> : public BasicTypeInfo
{
public:
    virtual std::string name() { return "string"; }

    virtual std::wstring ToString(void* pField)
    {
        return ((std::wstring*)pField)->c_str();
    }

    virtual void FromString(void* pField, const wchar_t* value)
    {
        *((std::wstring*)pField) = value;
    }

    virtual void* GetRawPtr(void* pField)
    {
        return &((std::wstring*)pField)->at(0);
    }

    virtual size_t GetRawSize(void* pField)
    {
        return sizeof(wchar_t) * ((std::wstring*)pField)->length();
    }

    virtual void SetRawSize(void* pField, size_t size)
    {
        ((std::wstring*)pField)->resize(size / sizeof(wchar_t));
    }

    virtual size_t GetFixedSize()
    {
        return 0;
    }

    virtual size_t GetSizeOfType()
    {
        return sizeof(std::wstring);
    }
};


template <>
class BasicTypeInfoT<std::string> : public BasicTypeInfo
{
public:
    virtual std::string name() { return "string"; }

    virtual std::wstring ToString(void* pField)
    {
        return pugi::as_wide(((std::string*)pField)->c_str());
    }

    virtual void FromString(void* pField, const wchar_t* value)
    {
        auto& s = *((std::string*)pField);
        s = pugi::as_utf8(value);
    }

    virtual void* GetRawPtr(void* pField)
    {
        return &((std::string*)pField)->at(0);
    }

    virtual size_t GetRawSize(void* pField)
    {
        return ((std::string*)pField)->length();
    }

    virtual void SetRawSize(void* pField, size_t size)
    {
        ((std::string*)pField)->resize(size);
    }

    virtual size_t GetFixedSize()
    {
        return 0;
    }

    virtual size_t GetSizeOfType()
    {
        return sizeof(std::string);
    }
};


template <>
class BasicTypeInfoT<int> : public BasicTypeInfo
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

    virtual size_t GetFixedSize()
    {
        return sizeof(int);
    }

    virtual size_t GetSizeOfType()
    {
        return sizeof(int);
    }
};

template <typename T>
class BasicStdTypeInfoT: public BasicTypeInfo
{
public:
    static const wchar_t* fmt;
    static const char* typeName;

    virtual std::string name()
    {
        if (typeName)
            return typeName;

        return getTypeName<T>();
    }

    virtual std::wstring ToString(void* pField)
    {
        T* p = (T*)pField;
        auto s = std::to_string(*p);
        return pugi::as_wide(s.c_str());
    }

    virtual void FromString(void* pField, const wchar_t* value)
    {
        #ifdef _WIN32
            swscanf_s(value, fmt, pField);
        #else
            swscanf(value, fmt, pField);
        #endif
    }

    virtual size_t GetFixedSize()
    {
        return sizeof(T);
    }

    virtual size_t GetSizeOfType()
    {
        return sizeof(T);
    }
};

template <>
class BasicTypeInfoT<int64_t> : public BasicStdTypeInfoT<int64_t> { };


template <>
class BasicTypeInfoT<bool> : public BasicTypeInfo
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

    virtual size_t GetFixedSize()
    {
        return sizeof(bool);
    }

    virtual size_t GetSizeOfType()
    {
        return sizeof(bool);
    }
};

// Just an example how primitive data types could be redefined. Serialize bool as "True/False" instead of "true/false".
class CamelCaseBool
{
public:
    bool value;
};

template <>
class BasicTypeInfoT<CamelCaseBool> : public BasicTypeInfoT<bool>
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
        BasicTypeInfoT<bool>::FromString(&((CamelCaseBool*)pField)->value, value);
    }

    virtual size_t GetFixedSize()
    {
        return sizeof(CamelCaseBool);
    }

    virtual size_t GetSizeOfType()
    {
        return sizeof(CamelCaseBool);
    }
};


template <class E>
class BasicTypeInfoT< std::vector<E> > : public BasicTypeInfo
{
public:
    std::shared_ptr<BasicTypeInfo> elementType;

    BasicTypeInfoT()
    {
        elementType.reset(new BasicTypeInfoT<E>());
    }

    virtual std::string name()
    {
        return getTypeName<std::vector<E>>();
    }

    virtual bool GetArrayElementType(BasicTypeInfo*& type)
    {
        type = nullptr;
        ClassTypeInfo* tinfo = nullptr;
        // If it's complex class type, return it
        CallToGetType<E>(tinfo);
        if (tinfo)
            type = dynamic_cast<BasicTypeInfo*>(tinfo);

        // Otherwise collect type info from element itself.
        if (type == nullptr)
            type = elementType.get();

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

    virtual std::wstring ToString(void*)
    {
        return std::wstring();
    }

    virtual void* GetRawPtr(void* p)
    {
        std::vector<E>* v = (std::vector<E>*)p;

        if (v->empty())
            return nullptr;

        return &v->at(0);
    }

    virtual size_t GetRawSize(void* p)
    {
        std::vector<E>* v = (std::vector<E>*)p;
        return sizeof(E) * v->size();
    }

    virtual void SetRawSize(void* p, size_t size)
    {
        std::vector<E>* v = (std::vector<E>*)p;
        size_t count = size / sizeof(E);
        v->resize(count);
    }

    virtual size_t GetFixedSize()
    {
        return 0;
    }

    virtual size_t GetSizeOfType()
    {
        return sizeof( std::vector<E> );
    }
};

