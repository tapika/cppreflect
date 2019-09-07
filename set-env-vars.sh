
if [ $TRAVIS_OS_NAME == "windows" ]; then
        DEPLOY_FILE=test_cppreflect.exe
else
        DEPLOY_FILE=test_cppreflect
fi

echo ---------------------------------------------------------------------------
echo DEPLOY_FILE: $DEPLOY_FILE
echo ---------------------------------------------------------------------------

env | sort
