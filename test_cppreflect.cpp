#include "cppreflect/cppreflect.h"

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
        (bool) isAdult
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


int main(void) 
{
    People ppl;
    ppl.groupName = "Group1";

    Person p;
    p.name = L"Roger";
    p.age = 37;
    p.gender = gender_male;
    p.isAdult = true;
    ppl.people.push_back(p);
    p.name = L"Alice";
    p.gender = gender_female;
    p.age = 27;
    p.isAdult = true;
    ppl.people.push_back(p);
    p.name = L"Cindy";
    p.gender = gender_female;
    p.age = 17;
    p.isAdult = false;
    ppl.people.push_back(p);

    wstring err;
    if (!SaveToXmlFile(L"peopleInfo.xml", &ppl, People::GetType(), err))
    {
        wprintf(L"Error: %ls\n", err.c_str());
        return 2;
    }

    wstring xml1 = as_xml(&ppl, People::GetType());
    wprintf(L"Serialized:\n%ls\n", xml1.c_str() );

    People ppl2;
    if (!LoadFromXmlFile(L"peopleInfo.xml", &ppl2, People::GetType(), err))
    {
        wprintf(L"Error: %ls\n", err.c_str());
        return 2;
    }
    wstring xml2 = as_xml(&ppl2, People::GetType());
    if (xml1 != xml2)
        printf("serialization/deserialization failed");

    return 0;
}

