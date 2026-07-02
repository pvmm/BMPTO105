#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <iostream>
#include <cstdlib>

#include "bmpto105_lib.hpp"

namespace py = pybind11;

class BmpTo105_ModuleEngine: public BmpTo105 {
public:
    //MSX105Bitmap convertFromPy(py::object object) {
    py::object convertFromPy(py::object object) {
        std::cout << "Got it!\n";
        if (!py::isinstance<py::object>(object)) {
            throw std::runtime_error("Object is not an RGBBitmap");
        }
        py::object widthAttr = object.attr("width");
        int width = py::cast<int>(widthAttr);
        (void)width;

        py::object heightAttr = object.attr("height");
        int height = py::cast<int>(heightAttr);
        (void)height;

        py::object channelsAttr = object.attr("channels");
        int channels = py::cast<int>(channelsAttr);
        (void)channels;

        py::object dataAttr = object.attr("data");
        py::list pyData = py::cast<py::list>(dataAttr);

        std::vector<uint8_t> data;
        data.reserve(pyData.size());

        for (size_t i = 0; i < pyData.size(); ++i) {
            try {
                int value = py::cast<int>(pyData[i]);
                if (value < 0 || value > 255) {
                    throw std::runtime_error("Value out of uint8_t range (0-255)");
                }
                data.push_back(static_cast<uint8_t>(value));
            } catch (const py::cast_error& e) {
                throw std::runtime_error("Failed to convert list element to int");
            }
        }

        // the actual conversion
        auto  rgbBitmap = RGBBitmap(width, height, channels, data);
        auto* msxBitmap = convertImage(rgbBitmap);

        py::list bitmap(msxBitmap->width * msxBitmap->height);
        for (uint32_t i = 0; i < msxBitmap->width * msxBitmap->height; ++i) {
            bitmap.append(msxBitmap->bitmap[i]);
        }

        // get a hold on the Python MSX105Bitmap class
        py::module_ bmpto105 = py::module_::import("bmpto105");
        py::object MSX105Bitmap_class = bmpto105.attr("MSX105Bitmap");

        // Create Python object with data
        auto result = MSX105Bitmap_class(
            msxBitmap->width,
            msxBitmap->height,
            bitmap
        );

	std::free(msxBitmap);
	return result;
    }
};

// The binding code
PYBIND11_MODULE(bmpto105, m) {
    py::class_<RGBBitmap>(m, "RGBBitmap")
        .def(py::init<int, int, int, std::vector<uint8_t>>())
        .def_readwrite("width", &RGBBitmap::width)
        .def_readwrite("height", &RGBBitmap::height)
        .def_readwrite("channels", &RGBBitmap::channels)
        .def_readwrite("data", &RGBBitmap::data);

    py::class_<MSX105Bitmap>(m, "MSX105Bitmap")
        .def(py::init<int, int>())
        .def_readwrite("width", &MSX105Bitmap::width)
        .def_readwrite("height", &MSX105Bitmap::height);

    py::class_<BmpTo105_ModuleEngine>(m, "BmpTo105")
        .def(py::init<const std::array<uint32_t, 16>&>(),
             py::arg("palette"),
             "Construct the engine with a persistent MSX color palette")
        .def("convert", &BmpTo105_ModuleEngine::convertFromPy,
             py::arg("image"),
             "Convert RGBBitmap to MSX105Bitmap using the palette");

    m.doc() = "Convert any bitmap format to 105 colors mode (MSX)";

    //m.def("add", [](int a, int b) { return a + b; }, "A function that adds two numbers");
}
