#pragma once
#include "macrohelpers.h"             //DOFOREACH_SEMICOLON
#include <memory>                     //shared_ptr
#include <vector>
#include <string>

class FieldInfo;
class ReflectClass;

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif

class ClassTypeInfo;
typedef ClassTypeInfo& (*pfuncGetClassInfo)();

//
//  Base class for performing field conversion to string / from string.
//
class BasicTypeInfo
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
    //  Gets class type if any
    //
    virtual BasicTypeInfo* GetClassType()
    {
        return nullptr;
    }

    //
    // Converts instance pointers to ReflectClass*.
    //
    virtual ReflectClass* ReflectClassPtr(void*)
    {
        return nullptr;
    }

    //
    // Gets array element type, in case if type is complex (class)
    //
    virtual bool GetArrayElementType(BasicTypeInfo*&)
    {
        // Not array type
        return false;
    }

    //
    // If GetArrayElementType() returns true, returns size of array.
    //
    virtual size_t ArraySize(void*)
    {
        return 0;
    }

    virtual void SetArraySize(void*, size_t)
    {
    }

    //
    //  Gets field (at p) array element at position i.
    //
    virtual void* ArrayElement(void*, size_t)
    {
        return nullptr; // Invalid operation, since not array
    }

    //
    // Converts specific data to String.
    //
    // Default implementation: Don't know how to print given field, ignore it
    //
    virtual std::wstring ToString(void*)
    {
        return std::wstring();
    }

    //
    // Converts from String to data.
    //
    // Default implementation: Value cannot be set from string.
    //
    virtual void FromString(void*, const wchar_t*)
    {
    }

    //
    // Returns raw accessor pointer to field, data
    //
    virtual void* GetRawPtr(void* pField)
    {
        return pField;
    }

    //
    // Returns element size totally so it can be streamed to buffer. For vector it's size of array * size of element.
    //
    virtual size_t GetRawSize(void*)
    {
        return 0;
    }

    //
    // Sets dynamic array size, previously obtained from GetRawSize()
    //
    virtual void SetRawSize(void*, size_t)
    {
    }

    //
    // Returns sizeof(type) if type is fixed size, 0 otherwise
    //
    virtual size_t GetFixedSize() = 0;
};

class ClassTypeInfo : public BasicTypeInfo
{
public:
    //  Type (class) name
    std::string name;
    std::vector<FieldInfo> fields;

    // Gets field by name, nullptr if not found.
    FieldInfo* GetField(const char* name);

    // Get field index, -1 if not found.
    int GetFieldIndex(const char* name);

    //  Creates new class instance, converts it to ReflectClass* base class
    virtual ReflectClass* ReflectCreateInstance() = 0;

    //  Creates new class instance, tries to convert it to T - if ok, assigns it to shared_ptr
    template <class T>
    void ReflectCreateInstance( std::shared_ptr<T>& ptr )
    {
        ReflectClass* base = ReflectCreateInstance();
        T* t = dynamic_cast<T*>(base);
        if(!t)
        {
            delete base;
            return;
        }

        ptr.reset(t);
    }
};

class ReflectClassTypeNameInfo
{
public:
    std::string ClassTypeName;
    ReflectClassTypeNameInfo( pfuncGetClassInfo func, const std::string& className );
};

template <class T>
class ClassTypeInfoT : public ClassTypeInfo
{
    ReflectClass* ReflectCreateInstance()
    {
        return new T;
    }

    virtual size_t GetFixedSize()
    {
        return sizeof(T);
    }
};

#include "typetraits.h"

class BasicTypeInfo;
class FieldInfo
{
public:
    std::string name;
    FieldInfo() : 
        serializeAsAttribute(true)                  // Consumes less disk space.
    {
    }

    void SetName( const char* fieldName )
    {
        if( fieldName[0] == ' ' ) fieldName++;      // Result of define macro expansion, we fix it here.
        name = fieldName;
    }

    int offset;                                     // Field offset within a class instance
    bool serializeAsAttribute;                      // true to serialize as attribute, false as element
    std::shared_ptr<BasicTypeInfo> fieldType;          // Class for field conversion to string / back from string. We must use 'new' otherwise virtual table does not gets initialized.
};


#define PUSH_FIELD_INFO(x)                                      \
    fi.SetName( ARGNAME_AS_STRING(x) );                         \
    fi.offset = offsetof(_className, ARGNAME(x));               \
    fi.fieldType.reset(new BasicTypeInfoT< ARGTYPE(x) >());     \
    t.fields.push_back(fi);                                     \

/*
Before using this macro, you must define your own types conversion
classes, for example see template class BasicTypeInfoT.

If you get compilation error, then it makes sense to try out first
without REFLECTABLE define, so you can specify normal C++ field
in class first, then adapt it under REFLECTABLE.

Also if your field does not needs to be serialized, declare it outside
of REFLECTABLE define.

While declaring REFLECTABLE(className, 
                    (fieldType) fieldName
                               ^ keep a space in between
fieldType <> fieldName otherwise intellisense might not work.
*/
#define REFLECTABLE(className, ...)                             \
    /* Dump field types and names */                            \
    DOFOREACH_SEMICOLON(ARGPAIR,__VA_ARGS__)                    \
    /* typedef is accessable from PUSH_FIELD_INFO define */     \
    typedef className _className;                               \
                                                                \
    static ClassTypeInfo& GetType()                             \
    {                                                           \
        static ClassTypeInfoT<className> t;                     \
        if( t.name.length() ) return t;                         \
        t.name = classTypeNameInfo.ClassTypeName;               \
        FieldInfo fi;                                           \
        /* Dump offsets and field names */                      \
        DOFOREACH_SEMICOLON(PUSH_FIELD_INFO,__VA_ARGS__)        \
        return t;                                               \
    }                                                           \

std::string ToXML_UTF8( void* pclass, ClassTypeInfo& type );
std::wstring ToXML( void* pclass, ClassTypeInfo& type );

//
//  Serializes class instance to xml string.
//
template <class T>
std::string ToXML_UTF8( T* pclass )
{
    ClassTypeInfo& type = T::GetType();
    return ToXML_UTF8(pclass, type);
}

template <class T>
std::wstring ToXML( T* pclass )
{
    ClassTypeInfo& type = T::GetType();
    return ToXML( pclass, type );
}

void NodeToBinaryData(char*& buf, int* len, void* pclass, BasicTypeInfo& type);
bool BinaryDataToNode(char*& buf, int* left, void* pclass, BasicTypeInfo& type);

//
//  Queries size of encoded class
//
int getEncodedSize(void* pclass, ClassTypeInfo& type);

//
//  Encodes class to binary buffer
//  To decode class from binary, use BinaryDataToNode
//
void serialize_to_buffer(std::string& buf, void* pclass, ClassTypeInfo& type);
bool parse_from_buffer(const void* buf, int len, void* pclass, ClassTypeInfo& type);

bool FromXml( void* pclass, ClassTypeInfo& type, const wchar_t* xml, std::wstring& error );

//
//  Deserializes class instance from xml data. pclass must be valid instance where to fetch data.
//
template <class T>
bool FromXml( T* pclass, const wchar_t* xml, std::wstring& error )
{
    ClassTypeInfo& type = T::GetType();
    return FromXml(pclass, type, xml, error);
}

bool LoadFromXmlFile(const wchar_t* path, void* pclass, ClassTypeInfo& type, std::wstring& error);
bool SaveToXmlFile(const wchar_t* path, void* pclass, ClassTypeInfo& type, std::wstring& error);
std::wstring as_xml(void* pclass, ClassTypeInfo& type);

class ReflectClass;

//
//  One step in whole <main class, sub-class, sub-class, ....> scenario
//
class ReflectPathStep
{
public:
    //
    //  reflectable class <this> pointer converted to ReflectClass. Restore original pointer by calling instance[x]->ReflectGetInstance(). 
    //
    ReflectClass* instance;

    //
    // Property name
    //
    const char*   propertyName;

    //
    // Type information of instance
    //
    ClassTypeInfo*  typeInfo;
};


//
//  Path to highlight property set / get.
//
class ReflectPath
{
public:
    ReflectPath(ClassTypeInfo& type, const char* propertyName);
    
    void Init(ReflectClass* instance);
    
    std::vector<ReflectPathStep>  steps;
};


//
//  All classes which use C++ reflection should inherit from this base class.
//
class ReflectClass
{
protected:
    // Parent class, nullptr if don't have parent class.
    ReflectClass*   _parent;

public:
    // Property name under assignment. If empty - can be used to bypass structure (exists on API level, does not exists in file format level), if non-empty -
    // specifies fieldname to be registered on parent.
    std::string  propertyName;

    // Map field name to index (used when sorting fields)
    std::map<std::string, int> mapFieldToIndex;

    ReflectClass();

    //
    //  Use current class instance provided as parent to replicate <_parent> pointer
    //  of all children, recursively. parent can be also nullptr if topmost class,
    //
    void ReflectConnectChildren(ReflectClass* parent);

    //  Gets parent's ReflectClass which contains this type.
    inline ReflectClass* GetParent()
    {
        return _parent;
    }

    virtual ClassTypeInfo& GetInstType() = 0;
    virtual void* ReflectGetInstance() = 0;

    //  By default set / get property rebroadcats event to parent class
    void PushPathStep(ReflectPath& path);
    virtual void OnBeforeGetProperty(ReflectPath& path);
    virtual void OnAfterSetProperty(ReflectPath& path);
};

template <class T>
class ReflectClassT : public ReflectClass
{
public:
    virtual ClassTypeInfo& GetInstType()
    {
        return T::GetType();
    }

    virtual void* ReflectGetInstance()
    {
        return (T*) this;
    }

    virtual size_t GetFixedSize()
    {
        return sizeof(T);
    }

    static ReflectClassTypeNameInfo classTypeNameInfo;
};

template <class T>
ReflectClassTypeNameInfo ReflectClassT<T>::classTypeNameInfo(&T::GetType, getTypeName<T>());
