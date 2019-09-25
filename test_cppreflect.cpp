#include "cppreflect/cppreflect.h"
#include <chrono>

using namespace std;

DECLARE_ENUM( EGender, "gender_",
    gender_male,
    gender_female
);

class Person: public ReflectClassT<Person>
{
public:
    REFLECTABLE(Person,
        (wstring) name,
        (EGender) gender,
        (int) age,
        (bool) isAdult,
        (std::vector<int>) childrenAges,
        (std::vector<string>) hobbies
    )
};

class People : public ReflectClassT<People>
{
public:
    REFLECTABLE(People,
        (string) groupName,
        (vector<Person>) people
    )
};

class Record : public ReflectClassT<Record>
{
public:
    REFLECTABLE(Record, 
        (std::vector<int64_t>) ids, 
        (std::vector<std::string>) strings
    );
};

#define TEST_SET1
#define TEST_SET2

int main(void) 
{
#ifdef TEST_SET1
    People ppl;
    ppl.groupName = "Group1";

    Person p;
    p.name = L"Roger";
    p.age = 37;
    p.gender = gender_male;
    p.isAdult = true;
    p.hobbies.push_back("fishing");
    p.hobbies.push_back("reading books");
    ppl.people.push_back(p);

    p = Person();
    p.name = L"Alice";
    p.gender = gender_female;
    p.age = 27;
    p.isAdult = true;
    p.childrenAges.push_back(1);
    p.childrenAges.push_back(3);
    p.childrenAges.push_back(5);
    p.hobbies.push_back("reading books");
    ppl.people.push_back(p);

    p = Person();
    p.name = L"Cindy";
    p.gender = gender_female;
    p.age = 17;
    p.isAdult = false;
    ppl.people.push_back(p);

    ClassTypeInfo& PeopleType = People::GetType();
    
    wstring err;
    if (!SaveToXmlFile(L"peopleInfo.xml", &ppl, PeopleType, err))
    {
        wprintf(L"Error: %ls\n", err.c_str());
        return 2;
    }

    wstring xml1 = as_xml(&ppl, PeopleType);
    wprintf(L"Serialized:\n%ls\n", xml1.c_str() );

    People ppl2;
    if (!LoadFromXmlFile(L"peopleInfo.xml", &ppl2, PeopleType, err))
    {
        wprintf(L"Error: %ls\n", err.c_str());
        return 2;
    }
    wstring xml2 = as_xml(&ppl2, PeopleType);
    if (xml1 != xml2)
        printf("serialization/deserialization failed");

    string pplbuf;
    serialize_to_buffer(pplbuf, &ppl2, PeopleType);
    People ppl3;
    parse_from_buffer(&pplbuf[0], pplbuf.size(), &ppl3, PeopleType);

    wstring xml3 = as_xml(&ppl3, PeopleType);
    if (xml2 != xml3)
        printf("serialization/deserialization failed");
#endif //TEST_SET1

    Record r1, r2;

    r1.ids.push_back(1);
    r1.ids.push_back(2);
    r1.ids.push_back(3);

    const std::string kStringValue
            = "shgfkghsdfjhgsfjhfgjhfgjsffghgsfdhgsfdfkdjhfioukjhkfdljgdfkgvjafdhasgdfwurtjkghfsdjkfg";
    r1.strings.push_back(kStringValue);

    ClassTypeInfo& RecordType = Record::GetType();
    string s;

#ifdef TEST_SET2

    serialize_to_buffer(s, &r1, RecordType);
    parse_from_buffer(&s[0], s.size(), &r2, RecordType);

    wstring xr1 = as_xml(&r1, RecordType);
    wstring xr2 = as_xml(&r2, RecordType);
    if (xr1 != xr2)
        printf("serialization/deserialization failed");

#endif 

    auto start = std::chrono::high_resolution_clock::now();
    const int iterations = 10000;

    for (size_t i = 0; i < iterations; i++) {

        serialize_to_buffer(s, &r1, RecordType);
        parse_from_buffer(&s[0], s.size(), &r2, RecordType);
    }

    auto finish = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(finish - start).count();


    return 0;
}

