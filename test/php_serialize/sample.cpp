#include <terark/ptree.hpp>

int main() {
	using namespace terark::objbox;
 // php_array is a HashMap<obj_ptr, obj_ptr>, keys must be integer or string
	php_array root;

	root[obj_ptr("abc")] = new obj_long(123);
	root[obj_ptr("str")] = new obj_string("html page content");
	root[obj_ptr(12345)] = new obj_array; // obj_array is a real array
	root[obj_ptr(6789 )] = new php_array;
	root[obj_ptr(67890)] = new php_array;
	root[obj_ptr(1L)] = new php_array;
	obj_cast<php_array>(root[obj_ptr(short(1))])[obj_ptr("xyz")] = new obj_short(1234);
	obj_array& arr0 = obj_cast<obj_array>(root[obj_ptr(12345)]);
	php_array& arr1 = obj_cast<php_array>(root[obj_ptr(67890)]);
	arr1[obj_ptr("xyz")] = new obj_array;
	arr0.push_back(obj_ptr(1));
	arr0.push_back(obj_ptr(2));
	arr0.push_back(obj_ptr(4));
	arr0.push_back(obj_ptr(9));
	arr0.push_back(obj_ptr(16));
	size_t f;
	f = root.find_i(obj_ptr(20L)); assert(root.end_i() == f);
	f = root.find_i(obj_ptr(20L)); assert(root.end_i() == f);
	f = root.find_i(obj_ptr("b")); assert(root.end_i() == f);
	f = root.find_i(obj_ptr("b")); assert(root.end_i() == f);
	std::string out;
	php_save(&root, &out);
	out.append("\n");
	::write(STDOUT_FILENO, out.data(), out.size());
	return 0;
}

