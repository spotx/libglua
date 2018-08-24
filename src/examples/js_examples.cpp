#include <glua/FileUtil.h>
#include <glua/GluaV8.h>

auto glua_print(std::string data) -> void
{
    std::cout << data << std::endl;
}

auto test_int_array(std::vector<int> arr_int) -> std::vector<int>
{
    std::sort(arr_int.begin(), arr_int.end());

    std::cout << "C++ sorted array: { ";
    for (auto& val : arr_int)
    {
        std::cout << val << " ";
    }
    std::cout << "}" << std::endl;

    return arr_int;
}

auto test_bool_map(std::unordered_map<std::string, bool> map_bool) -> std::unordered_map<std::string, bool>
{
    for (auto& map_pair : map_bool)
    {
        map_pair.second = !map_pair.second;
    }

    return map_bool;
}

auto gimme_five() -> int
{
    return 5;
}

auto main(int argc, char* argv[]) -> int
{
    kdk::glua::GluaV8 glua{std::cout};

    ///////////// ADD CUSTOM BINDINDS HERE /////////////
    REGISTER_TO_GLUA(glua, glua_print);
    REGISTER_TO_GLUA(glua, test_int_array);
    REGISTER_TO_GLUA(glua, test_bool_map);
    REGISTER_TO_GLUA(glua, gimme_five);

    if (argc != 2)
    {
        std::cout << "Usage: " << argv[0] << " <path_to_example_js_script>" << std::endl;
    }
    else
    {
        std::string script = kdk::file_util::read_all(argv[1]);

        if (script.empty())
        {
            std::cout << "File [" << argv[1] << "] not found or was invalid" << std::endl;
        }

        glua.RunScript(script);
    }

    return 0;
}