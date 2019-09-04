#include <regex>
#include "cppreflect.h"
#include "pugixml/pugixml.hpp"              //pugi::xml_node
#include <sstream>                          //wstringstream

using namespace pugi;
using namespace std;

std::map< std::string, pfuncGetClassInfo > CppTypeInfo::classNameToClassInfo;


FieldInfo* CppTypeInfo::GetField(const char* name)
{
    for( auto& f: fields )
        if(f.name == name)
            return &f;

    return nullptr;
}

int CppTypeInfo::GetFieldIndex(const char* name)
{
    for( size_t i = 0; i < fields.size(); i++ )
        if (fields[i].name == name )
            return (int)i;

    return -1;
}

ReflectClassTypeNameInfo::ReflectClassTypeNameInfo(pfuncGetClassInfo func, const std::string& className)
{
    ClassTypeName = className;
    CppTypeInfo::classNameToClassInfo[className] = func;
}

//
//  Serializes class instance to xml node.
//
bool DataToNode( xml_node& _node, void* pclass, bool appendTypeName, CppTypeInfo& type )
{
    xml_node node;
    
    if(appendTypeName)
        node = _node.append_child(as_wide(type.name).c_str());
    else
        node = _node;

    for (FieldInfo fi : type.fields)
    {
        void* p = ((char*)pclass) + fi.offset;
        CppTypeInfo* arrayType = nullptr;

        if( !fi.fieldType->GetArrayElementType( arrayType ) )
        {
            // Primitive data type (string, int, bool)
            if (fi.fieldType->IsPrimitiveType())
            {
                // Simple type, append as attribute.
                auto s = fi.fieldType->ToString(p);
                if (!s.length())     // Don't serialize empty values.
                    continue;
                
                wstring fi_name = as_wide(fi.name);
                if (fi.serializeAsAttribute)
                    node.append_attribute(fi_name.c_str()) = s.c_str();
                else
                    node.append_child(fi_name.c_str()).append_child(pugi::node_pcdata).set_value(s.c_str());
            }
            else
            {
                // Complex class type, append as xml.
                xml_node fieldNode = node.append_child(as_wide(fi.name).c_str());
                DataToNode(fieldNode, p, false, *fi.fieldType->GetFieldType());
            }
            continue;
        }

        if( !arrayType )
            continue;

        size_t size = fi.fieldType->ArraySize(p);
        // Don't create empty arrays
        if (size == 0)
            continue;

        xml_node fieldNode = node.append_child(as_wide( fi.name ).c_str() );

        for( size_t i = 0; i < size; i++ )
        {
            void* pstr2 = fi.fieldType->ArrayElement( p, i );
            DataToNode( fieldNode, pstr2, true, *arrayType );
        }
    } //for each

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

string ToXML_UTF8( void* pclass, CppTypeInfo& type )
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

wstring ToXML( void* pclass, CppTypeInfo& type )
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
bool NodeToData( xml_node node, void* pclass, CppTypeInfo& type, bool typeCheck, wstring& error )
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

    for (FieldInfo fi: type.fields)
    {
        void* p = ((char*)pclass) + fi.offset;
        CppTypeInfo* arrayType = nullptr;

        if( !fi.fieldType->GetArrayElementType( arrayType ) )
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
            }
            else
            {
                // Complex class
                xml_node fieldNode = node.child(as_wide(fi.name).c_str());
                if (fieldNode.empty())
                    continue;

                if (!NodeToData(fieldNode, p, *fi.fieldType->GetFieldType(), false, error))
                    return false;
            }
            continue;
        }

        if( !arrayType )
            continue;

        xml_node fieldNode = node.child( as_wide( fi.name ).c_str());
        if( fieldNode.empty() )
            continue;

        int size = 0;
        for( auto it = fieldNode.children().begin(); it != fieldNode.children().end(); it++ )
            size++;

        fi.fieldType->SetArraySize( p, size );
        int i = 0;
        for( auto it = fieldNode.children().begin(); it != fieldNode.children().end(); it++ )
        {
            void* pstr2 = fi.fieldType->ArrayElement( p, i );
            if( !NodeToData( *it, pstr2, *arrayType, true, error ) )
                return false;
            i++;
        }
    } //for each

    return true;
}

bool FromXml( void* pclass, CppTypeInfo& type, const wchar_t* xml, wstring& error )
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

bool SaveToXmlFile(const wchar_t* path, void* pclass, CppTypeInfo& type, std::wstring& error)
{
    xml_document doc;

    xml_node decl = doc.prepend_child(pugi::node_declaration);
    decl.append_attribute(L"version") = L"1.0";
    decl.append_attribute(L"encoding") = L"utf-8";

    if (!DataToNode(doc, pclass, true, type))
        return false;

    return doc.save_file(path, L"  ", format_indent | format_save_file_text | format_write_bom, encoding_utf8);
}

std::wstring as_xml(void* pclass, CppTypeInfo& type)
{
    xml_document doc;

    if (!DataToNode(doc, pclass, true, type))
        return L"";

    std::wstringstream ss;
    doc.save(ss, L"  ", format_indent | format_no_declaration);
    return ss.str();
}


bool LoadFromXmlFile(const wchar_t* path, void* pclass, CppTypeInfo& type, std::wstring& error)
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


ReflectPath::ReflectPath(CppTypeInfo& type, const char* _propertyName)
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

void ReflectClass::ReflectConnectChildren(ReflectClass* parent)
{
    CppTypeInfo& typeinfo = GetInstType();
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





