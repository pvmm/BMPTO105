#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>
#include <iostream>
#include <cstdlib>
#include <tuple>
#include <cstdint>
#include <memory>
#include <filesystem>

#include "libbmpto105.hpp"

namespace py = pybind11;
static py::object datatypes_module;

#include <Python.h>
#include <filesystem>

namespace fs = std::filesystem;

// Helper: Convert py::sequence to std::span
template<typename T>
std::span<const T> sequence_to_span(const py::sequence& seq)
{
	py::array array = py::array::ensure(seq);
	if (!array) {
		throw std::runtime_error("Sequence could not be converted to a contiguous array");
	}

	py::buffer_info info = array.request();

	if (info.format != py::format_descriptor<T>::format()) {
		throw std::runtime_error("Element type mismatch");
	}

	return std::span<const T>(
		static_cast<const T*>(info.ptr),
		info.size
	);
}

// Helper to convert tuple sequence to array of 16 tuples (fixed size)
std::vector<std::tuple<uint8_t, uint8_t, uint8_t>> sequence_to_tuple_vector(const py::sequence& seq)
{
	std::vector<std::tuple<uint8_t, uint8_t, uint8_t>> result;
	result.reserve(py::len(seq));

	for (auto item : seq) {
		if (!py::isinstance<py::tuple>(item)) {
			throw std::runtime_error("Each palette element must be a tuple");
		}
		py::tuple t = py::cast<py::tuple>(item);
		if (py::len(t) != 3) {
			throw std::runtime_error("Each palette tuple must have exactly 3 values");
		}
		// convert tuple to bytes to trigger error
		py::object bytes_built_in = py::module_::import("builtins").attr("bytes");
		bytes_built_in(t).cast<py::bytes>();

		result.push_back(
			std::make_tuple(
				py::cast<uint8_t>(t[0]),
				py::cast<uint8_t>(t[1]),
				py::cast<uint8_t>(t[2])
			)
		);
	}
	return result;
}

std::vector<uint8_t> tuple_sequence_to_vector(const py::sequence& seq)
{
	std::vector<uint8_t> result;
	result.reserve(py::len(seq) * 3);

	for (auto item : seq) {
		if (!py::isinstance<py::tuple>(item)) {
			throw std::runtime_error("Each palette element must be a tuple");
		}
		py::tuple t = py::cast<py::tuple>(item);
		if (py::len(t) != 3) {
			throw std::runtime_error("Each palette tuple must have exactly 3 values");
		}
		result.push_back(py::cast<uint8_t>(t[0]));
		result.push_back(py::cast<uint8_t>(t[1]));
		result.push_back(py::cast<uint8_t>(t[2]));
	}
	return result;
}

// Helper to convert sequence to array of 16 uint32_t
std::array<uint32_t, 16> sequence_to_uint32_array(const py::sequence& seq) {
	if (py::len(seq) != 16) {
		throw std::runtime_error("Palette must have exactly 16 elements");
	}

	std::array<uint32_t, 16> result;
	size_t idx = 0;

	for (auto item : seq) {
		result[idx] = py::cast<uint32_t>(item);
		idx++;
	}
	return result;
}

// Module Engine class
class BmpTo105_ModuleEngine : public BmpTo105 {
public:
	// Constructor from tuple sequence
	BmpTo105_ModuleEngine(const py::sequence& palette_)
		: BmpTo105(sequence_to_tuple_vector(palette_)) {
		std::cout << "Engine created from tuple sequence\n";
	}

	// Constructor from uint32_t sequence
	BmpTo105_ModuleEngine(const py::array_t<uint32_t>& palette_arr) {
		py::buffer_info info = palette_arr.request();
		if (info.size != 16) {
			throw std::runtime_error("Palette must have exactly 16 elements");
		}
		uint32_t* data = static_cast<uint32_t*>(info.ptr);
		std::span<const uint32_t, 16> palette_span(data, 16);

		// Call base constructor (we need to reinitialize)
		// Since we can't call base constructor after the fact, use a helper
		// Or better: store palette and initialize properly
		std::array<uint32_t, 16> palette_arr_static;
		std::copy(data, data + 16, palette_arr_static.begin());
		// Manually initialize using the base class's init function
		initPalette(std::span<const uint32_t, 16>(palette_arr_static));
		createColorCombo(palette, colorComboTable);
		std::cout << "Engine created from uint32_t array\n";
	}

	// Convert method that returns a Python object
	py::object convertFromPy(py::object object) {
		if (!py::isinstance<py::object>(object)) {
			throw std::runtime_error("Object is not an RGBBitmap");
		}

		// Extract attributes
		py::object widthAttr = object.attr("width");
		int width = py::cast<int>(widthAttr);

		py::object heightAttr = object.attr("height");
		int height = py::cast<int>(heightAttr);

		// Call the method to get flattened data
		py::object dataResult = object.attr("get_flattened_data")();

		// Ensure the result is a sequence
		if (!py::isinstance<py::sequence>(dataResult)) {
			throw std::runtime_error("get_flattened_data() must return a sequence");
		}
		py::sequence dataSeq = py::cast<py::sequence>(dataResult);
		auto data = tuple_sequence_to_vector(dataSeq);

		// Create RGBBitmap and convert
		RGBBitmap rgbBitmap(width, height, 3 /* channels */, data);
		MSXBitmap* msxBitmap = convertImage(rgbBitmap);

		// Convert MSX bitmap data to Python list
		py::list bitmap;
		size_t numPixels = msxBitmap->width * height;

		for (size_t i = 0; i < numPixels; ++i) {
			bitmap.append(msxBitmap->bitmap[i].c0);
			bitmap.append(msxBitmap->bitmap[i].p0);
			bitmap.append(msxBitmap->bitmap[i].c1);
			bitmap.append(msxBitmap->bitmap[i].p1);
		}

		// Create Python palette
		py::object RGBColor_class = datatypes_module.attr("RGBColor");
		py::list pylette;
		for (const RGBColor& c: palette) {
			pylette.append(RGBColor_class(c.r, c.g, c.b));
		}

		// Create Python object with data
		py::object MSXBitmap_class = datatypes_module.attr("MSXBitmap");
		py::object result = MSXBitmap_class(
			msxBitmap->width,
			height,
			pylette,
			bitmap
		);

		std::free(msxBitmap);
		return result;
	}
};

// The binding code
PYBIND11_MODULE(libbmpto105, m) {
	// access .datatype submodule from C++ code
	py::object current_package = m.attr("__package__");
	py::object import_module = py::module_::import("importlib").attr("import_module");
	datatypes_module = import_module(".datatypes", current_package);

	m.doc() = "Convert any bitmap format to 105 colors mode (MSX)";

	// Bind RGBBitmap
	py::class_<RGBBitmap>(m, "_RGBBitmap")
		.def(py::init<int, int, int, const std::vector<uint8_t>&>())
		.def_readonly("width", &RGBBitmap::width)
		.def_readonly("height", &RGBBitmap::height)
		.def_readonly("data", &RGBBitmap::data);

	// Bind the engine
	py::class_<BmpTo105_ModuleEngine>(m, "BmpTo105")
		.def(py::init<const py::sequence&>(),
			 py::arg("palette"),
			 "Construct the engine with a palette (list of 16 RGB tuples or uint32_t values)")
		.def("convert", &BmpTo105_ModuleEngine::convertFromPy,
			 py::arg("image"),
			 "Convert RGBBitmap to MSXBitmap using the palette")
		.def("get_palette", &BmpTo105::getPalette,
			 "Get the current palette as a list of RGB colors");

	// called when python interpreter is shuting down
	auto atexit = py::module_::import("atexit");
	atexit.attr("register")(py::cpp_function([]() {
		datatypes_module.release();
	}));
}
