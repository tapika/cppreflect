
if [ $TRAVIS_OS_NAME == "windows" ]; then
        DEPLOY_FILE=$TRAVIS_BUILD_DIR/test_cppreflect.exe
else
        DEPLOY_FILE=$TRAVIS_BUILD_DIR/test_cppreflect
fi

export TRAVIS_TAG=1.0

echo ---------------------------------------------------------------------------
echo DEPLOY_FILE: $DEPLOY_FILE
echo ---------------------------------------------------------------------------

env | sort
