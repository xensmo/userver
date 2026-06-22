project(sqldto-golden-tests)

add_test(
    NAME sqldto-golden
    COMMAND "${USERVER_SQLDTO_PYTEST_PYTHON_BINARY}" tests/golden_tests/main.py
            --python "${USERVER_SQLDTO_PYTEST_PYTHON_BINARY}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../.."
)

add_custom_target(
    NAME sqldto-golden-update
    COMMAND "${USERVER_SQLDTO_PYTEST_PYTHON_BINARY}" tests/golden_tests/main.py
            --python "${USERVER_SQLDTO_PYTEST_PYTHON_BINARY}"
            --canonize-tests
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../.."
)
