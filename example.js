glua_print("Printing from glua!");
glua_print("2Printing from glua!");
glua_print("3Printing from glua!");

var int_array = [Math.random() * 1000, Math.random() * 1000, Math.random() * 1000, Math.random() * 1000, Math.random() * 1000];
var sorted_int_array = test_int_array(int_array);

var arr_length = sorted_int_array.length;
for (var i = 0; i < arr_length; ++i) {
    glua_print("sorted item[" + i + "] = " + sorted_int_array[i]);
}

var result_map = test_bool_map({ herp: true, slerp: false, greatest: true });
for (var key in result_map) {
    glua_print("result_map[" + key + "] = " + result_map[key]);
}

glua_print(gimme_five());
glua_print(gimme_five());
glua_print(gimme_five());