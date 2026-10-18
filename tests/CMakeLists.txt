# CuteChat tests: QtTest, headless, no network (HTTP stub on a QTcpServer).
# Test names are kept as SupraFit knew them, so `ctest -R 'Tool|Llm|Chat|Config|Preset'` keeps working.
# Claude Generated 2026
function(cutechat_add_test target source testname)
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE cutechat_core Qt6::Test Qt6::Network)
    set_target_properties(${target} PROPERTIES AUTOMOC ON)
    target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME ${testname} COMMAND ${target})
endfunction()

cutechat_add_test(test_toolregistry test_toolregistry.cpp ToolRegistryTest)
cutechat_add_test(test_tooldispatcher test_tooldispatcher.cpp ToolDispatcherTest)
cutechat_add_test(test_llmclient test_llmclient.cpp LlmClientTest)
cutechat_add_test(test_llmsession test_llmsession.cpp LlmSessionTest)
cutechat_add_test(test_chathelp test_chathelp.cpp ChatHelpTest)
cutechat_add_test(test_chatengine test_chatengine.cpp ChatEngineTest)
cutechat_add_test(test_chatlog test_chatlog.cpp ChatLogTest)
cutechat_add_test(test_llmpresets test_llmpresets.cpp LlmConfigPresetTest)
cutechat_add_test(test_cutechat_extras test_cutechat_extras.cpp CuteChatExtrasTest)

if(CUTECHAT_BUILD_WIDGETS)
    add_executable(test_chatdock test_chatdock.cpp)
    target_link_libraries(test_chatdock PRIVATE cutechat Qt6::Test Qt6::Network Qt6::Widgets)
    set_target_properties(test_chatdock PROPERTIES AUTOMOC ON)
    target_include_directories(test_chatdock PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME ChatDockTest COMMAND test_chatdock)
    set_tests_properties(ChatDockTest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;HOME=${CMAKE_CURRENT_BINARY_DIR}/testhome")
endif()
