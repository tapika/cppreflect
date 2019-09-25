#include <regex>
#include "cppreflect.h"
#include "pugixml/pugixml.hpp"              //pugi::xml_node
#include <sstream>                          //wstringstream

using namespace pugi;
using namespace std;


FieldInfo* ClassTypeInfo::GetField(const char* name)
{
    for( auto& f: fields )
        if(f.name == name)
            return &f;

    return nullptr;
}

int ClassTypeInfo::GetFieldIndex(const char* name)
{
    for( size_t i = 0; i < fields.size(); i++ )
        if (fields[i].name == name )
            return (int)i;

    return -1;
}

ReflectClassTypeNameInfo::ReflectClassTypeNameInfo(pfuncGetClassInfo, const std::string& className)
{
    ClassTypeName = className;
}

//
//  Serializes class instance to xml node.
//
bool DataToNode( xml_node& _node, void* pclass, bool appendTypeName, ClassTypeInfo& type )
{
    xml_node node;
    
    if(appendTypeName)
        node = _node.append_child(as_wide(type.name).c_str());
    else
        node = _node;

    for (FieldInfo& fi : type.fields)
    {
        void* p = ((char*)pclass) + fi.offset;
        BasicTypeInfo* arrayType;
        BasicTypeInfo& fieldType = *fi.fieldType;

        if (!fieldType.GetArrayElementType(arrayType))
        {
            // Primitive data type (string, int, bool)
            if (fieldType.IsPrimitiveType())
            {
                // Simple type, append as attribute.
                auto s = fieldType.ToString(p);
                if (!s.length()) // Don't serialize empty values.
                    continue;

                wstring fi_name = as_wide(fi.name);
                if (fi.serializeAsAttribute)
                    node.append_attribute(fi_name.c_str()) = s.c_str();
                else
                    node.append_child(fi_name.c_str()).append_child(pugi::node_pcdata).set_value(s.c_str());
            } else {
                // Complex class type, append as xml.
                xml_node fieldNode = node.append_child(as_wide(fi.name).c_str());
                DataToNode(fieldNode, p, false, *((ClassTypeInfo*)fieldType.GetClassType()));
            }
            continue;
        }

        if (!arrayType)
            continue;

        size_t size = fieldType.ArraySize(p);
        // Don't create empty arrays
        if (size == 0)
            continue;

        xml_node fieldNode = node.append_child(as_wide(fi.name).c_str());
        ClassTypeInfo* classType = dynamic_cast<ClassTypeInfo*>(arrayType);
        wstring xmlNodeName;
        if (!classType)
            xmlNodeName = as_wide(arrayType->name());

        for (size_t i = 0; i < size; i++)
        {
            void* pstr2 = fieldType.ArrayElement(p, i);
            if (classType)
            {
                DataToNode(fieldNode, pstr2, true, *classType);
            }
            else
            {
                auto s = arrayType->ToString(pstr2);
                fieldNode.append_child(xmlNodeName.c_str()).append_child(pugi::node_pcdata).set_value(s.c_str());
            }
        }
    } // for each

    return true;
}

//  Helper class.
struct xml_string_writer : xml_writer
{
    string result;
    virtual void write( const void* data, size_t size )
    {
        result += string( (const char*)data, (int)size );
    }
};

string ToXML_UTF8( void* pclass, ClassTypeInfo& type )
{
    xml_document doc;
    xml_node decl = doc.prepend_child( pugi::node_declaration );
    decl.append_attribute( L"version" ) = L"1.0";
    decl.append_attribute( L"encoding" ) = L"utf-8";
    DataToNode( doc, pclass, true, type );

    xml_string_writer writer;
    doc.save( writer, L"  ");
    return writer.result;
}

wstring ToXML( void* pclass, ClassTypeInfo& type )
{
    xml_document doc;
    xml_node decl = doc.prepend_child( pugi::node_declaration );
    decl.append_attribute( L"version" ) = L"1.0";
    decl.append_attribute( L"encoding" ) = L"utf-8";
    DataToNode( doc, pclass, true, type );

    xml_string_writer writer;
    doc.save( writer, L"  ");
    return as_wide(writer.result.c_str());
}


//
//  Deserializes xml to class structure, returns true if succeeded, false if fails.
//  error holds error information if any.
//
bool NodeToData( xml_node node, void* pclass, ClassTypeInfo& type, bool typeCheck, wstring& error )
{
    string name = as_utf8(node.name());

    if(typeCheck && type.name != name )
    {
        error.append(L"Expected xml tag '");
        error.append(as_wide(type.name));
        error.append(L"', but found '");
        error.append(as_wide(name));
        error.append(L"'");
        return false;
    }

    for (FieldInfo& fi : type.fields)
    {
        void* p = ((char*)pclass) + fi.offset;
        BasicTypeInfo* arrayType;
        BasicTypeInfo& fieldType = *fi.fieldType;

        if (!fieldType.GetArrayElementType(arrayType))
        {
            // Primitive data type (string, int, bool)
            if (fi.fieldType->IsPrimitiveType())
            {
                // Simple type, query value from xml attribute or xml element.
                wstring fi_name = as_wide(fi.name);
                const wchar_t* v;
                if (fi.serializeAsAttribute)
                    v = node.attribute(fi_name.c_str()).value();
                else
                    v = node.child(fi_name.c_str()).child_value();

                fi.fieldType->FromString(p, v);
            } else {
                // Complex class
                xml_node fieldNode = node.child(as_wide(fi.name).c_str());
                if (fieldNode.empty())
                    continue;

                if (!NodeToData(fieldNode, p, *((ClassTypeInfo*)fi.fieldType->GetClassType()), false, error))
                    return false;
            }
            continue;
        }

        if (!arrayType)
            continue;

        xml_node fieldNode = node.child(as_wide(fi.name).c_str());
        if (fieldNode.empty())
            continue;

        ClassTypeInfo* classType = dynamic_cast<ClassTypeInfo*>(arrayType);
        wstring xmlNodeName;
        if (!classType)
        {
            xmlNodeName = as_wide(arrayType->name());
        }

        int size = 0;
        for (auto it = fieldNode.children().begin(); it != fieldNode.children().end(); it++)
            size++;

        fieldType.SetArraySize(p, size);

        int i = 0;
        for (auto it = fieldNode.children().begin(); it != fieldNode.children().end(); it++)
        {
            void* pstr2 = fieldType.ArrayElement(p, i);

            if (classType)
            {
                if (!NodeToData(*it, pstr2, *classType, true, error))
                    return false;
            }
            else
            {
                if (it->name() != xmlNodeName)
                {
                    error.append(L"Expected xml tag '");
                    error.append(xmlNodeName);
                    error.append(L"', but found '");
                    error.append(it->name());
                    error.append(L"'");
                    return false;
                }

                auto svalue = it->child_value();
                arrayType->FromString(pstr2, svalue);
            }

            i++;
        }
    } // for each

    return true;
}

void lengthToBinaryData(char*& buf, int* len, size_t& t)
{
    *len += sizeof(size_t);
    if (buf) {
        memcpy(buf, &t, sizeof(size_t));
        buf += sizeof(size_t);
    }
}

//
//  Serializes class instance to binary buffer.
//
//  *buf if non-null - receives encoded buffer
//  *len - buffer encoded length
//
void NodeToBinaryData(char*& buf, int* len, void* pclass, BasicTypeInfo& type)
{
    ClassTypeInfo* clstype = dynamic_cast<ClassTypeInfo*>(&type);

    if (!clstype) {
        // Primitive data type (string, int, bool)
        size_t s = type.GetFixedSize();

        if (s == 0) {
            s = type.GetRawSize(pclass);
            lengthToBinaryData(buf, len, s);
        }
        *len += (int)s;

        if (buf) {
            memcpy(buf, type.GetRawPtr(pclass), s);
            buf += s;
        }
        return;
    }

    for (FieldInfo& fi : clstype->fields)
    {

        void* p = ((char*)pclass) + fi.offset;
        BasicTypeInfo& fieldType = *fi.fieldType;
        BasicTypeInfo* arrayType = nullptr;

        if (!fieldType.GetArrayElementType(arrayType)) {
            NodeToBinaryData(buf, len, p, fieldType);
            continue;
        }

        size_t size = fieldType.ArraySize(p);
        lengthToBinaryData(buf, len, size);

        for (size_t i = 0; i < size; i++) {
            void* pstr2 = fi.fieldType->ArrayElement(p, i);
            NodeToBinaryData(buf, len, pstr2, *arrayType);
        }
    } // for each
}

//
//  Serializes class instance to xml node.
//
bool BinaryDataToNode(char*& buf, int* left, void* pclass, BasicTypeInfo& type)
{
    try {
        ClassTypeInfo* clstype = dynamic_cast<ClassTypeInfo*>(&type);

        if (!clstype) {
            // Primitive data type (string, int, bool)
            size_t s = type.GetFixedSize();
            size_t l = s;

            if (s == 0) {
                s = sizeof(size_t);
                if (*left < (int)s)
                    return false;

                memcpy(&l, buf, s);
                buf += s;
                *left -= (int)s;
                type.SetRawSize(pclass, l);
            }

            if (*left < (int)l)
                return false;
            memcpy(type.GetRawPtr(pclass), buf, l);
            buf += l;
            *left -= (int)l;
            return true;
        }

        for (FieldInfo& fi : clstype->fields) {

            void* p = ((char*)pclass) + fi.offset;
            BasicTypeInfo* arrayType = nullptr;
            BasicTypeInfo& fieldType = *fi.fieldType;

            if (!fieldType.GetArrayElementType(arrayType)) {

                if (!BinaryDataToNode(buf, left, p, fieldType))
                    return false;

                continue;
            }

            size_t s = sizeof(size_t);
            size_t arrSize = 0;
            if (*left < (int)s)
                return false;
            memcpy(&arrSize, buf, s);
            *left -= (int)s;
            buf += s;

            fieldType.SetArraySize(p, arrSize);

            char* pstr2;
            size_t elemSize = 0;

            if (arrSize != 0)
            {
                pstr2 = (char*)fieldType.ArrayElement(p, 0);
                elemSize = arrayType->GetSizeOfType();
            }

            // Primitive flat type, can be just copied.
            if (arrayType->GetFixedSize() != 0 && dynamic_cast<ClassTypeInfo*>(arrayType) == nullptr)
            {
                size_t copySize = arrSize * elemSize;
                if (*left < (int)copySize)
                    return false;

                if (buf)
                {
                    memcpy(pstr2, buf, copySize);
                    buf += copySize;
                }

                *left -= (int)copySize;
                continue;
            }

            // Complex type, requires for loop
            for (size_t i = 0; i < arrSize; i++)
            {
                if (!BinaryDataToNode(buf, left, pstr2, *arrayType))
                    return false;

                pstr2 += elemSize;
            }

        } // for each
    } catch (std::bad_alloc&) {
        // Either out of memory or incorrectly decoded buffer
        return false;
    }
    return true;
}

int getEncodedSize(void* pclass, ClassTypeInfo& type)
{
    int len = 0;
    char* buf = nullptr;
    NodeToBinaryData(buf, &len, pclass, type);

    return len;
}

void serialize_to_buffer(std::string& buf, void* pclass, ClassTypeInfo& type)
{
    buf.resize(getEncodedSize(pclass, type));
    char* pbuf = (char*)&buf.front();
    int len = 0;
    NodeToBinaryData(pbuf, &len, pclass, type);
}

bool parse_from_buffer(const void* buf, int len, void* pclass, ClassTypeInfo& type)
{
    char* pbuf = (char*)buf;
    int llen = len;

    return BinaryDataToNode(pbuf, &llen, pclass, type);
}

bool FromXml( void* pclass, ClassTypeInfo& type, const wchar_t* xml, wstring& error )
{
    xml_document doc2;

    xml_parse_result res = doc2.load_string( xml );
    if( !res )
    {
        error = L"Failed to load xml: ";
        error.append(as_wide(res.description()));
        return false;
    }

    return NodeToData( doc2.first_child(), pclass, type, true, error );
}

bool SaveToXmlFile(const wchar_t* path, void* pclass, ClassTypeInfo& type, std::wstring&)
{
    xml_document doc;

    xml_node decl = doc.prepend_child(pugi::node_declaration);
    decl.append_attribute(L"version") = L"1.0";
    decl.append_attribute(L"encoding") = L"utf-8";

    if (!DataToNode(doc, pclass, true, type))
        return false;

    return doc.save_file(path, L"  ", format_indent | format_save_file_text | format_write_bom, encoding_utf8);
}

std::wstring as_xml(void* pclass, ClassTypeInfo& type)
{
    xml_document doc;

    if (!DataToNode(doc, pclass, true, type))
        return L"";

    std::wstringstream ss;
    doc.save(ss, L"  ", format_indent | format_no_declaration);
    return ss.str();
}


bool LoadFromXmlFile(const wchar_t* path, void* pclass, ClassTypeInfo& type, std::wstring& error)
{
    xml_document doc2;

    xml_parse_result res = doc2.load_file(path);
    if (!res)
    {
        error = L"Failed to load xml: ";
        error.append(as_wide(res.description()));
        return false;
    }

    wstring error2;
    if (NodeToData(doc2.first_child(), pclass, type, true, error2))
        return true;

    error = error2;
    return false;
}


ReflectPath::ReflectPath(ClassTypeInfo& type, const char* _propertyName)
{
    // Doubt that class hierarchy is more complex than 5 levels, but increase this size if it's.
    steps.reserve(5);
    ReflectPathStep step;
    step.typeInfo = &type;
    step.propertyName = _propertyName;
    step.instance = nullptr;
    steps.push_back(step);
}

void ReflectPath::Init(ReflectClass* instance)
{
    steps.resize(1);
    steps[0].instance = instance;
}

ReflectClass::ReflectClass():
    _parent(nullptr)
{
}

void ReflectClass::ReflectConnectChildren(ReflectClass*)
{
    ClassTypeInfo& typeinfo = GetInstType();
    char* inst = nullptr;
    int idx = 0;
    
    for( auto& fi: typeinfo.fields )
    {
        mapFieldToIndex[fi.name] = idx;

        if( fi.fieldType->IsPrimitiveType() )
            continue;

        if( !inst )
            inst = (char*)ReflectGetInstance();

        ReflectClass* child = fi.fieldType->ReflectClassPtr(inst + fi.offset);
        child->_parent = this;

        // Reconnect children as well recursively.
        child->ReflectConnectChildren(this);

        if( child->propertyName.length() != 0 )
            mapFieldToIndex[child->propertyName.c_str()] = idx;

        idx++;

        if (child->propertyName.length() == 0)
        {
            for (auto fi : child->GetInstType().fields)
                mapFieldToIndex[fi.name] = idx++;
        }

    }
}

//
//  Pushes information about current path step
//
void ReflectClass::PushPathStep(ReflectPath& path)
{
    if (propertyName.length() == 0)
        return;

    ReflectPathStep step;
    step.typeInfo = &_parent->GetInstType();
    step.propertyName = propertyName.c_str();
    step.instance = _parent;
    path.steps.push_back(step);
}

void ReflectClass::OnBeforeGetProperty(ReflectPath& path)
{
    if(!_parent)
        return;

    PushPathStep(path);
    _parent->OnBeforeGetProperty(path);
}

void ReflectClass::OnAfterSetProperty(ReflectPath& path)
{
    if (!_parent)
        return;

    PushPathStep(path);
    _parent->OnAfterSetProperty(path);
}





