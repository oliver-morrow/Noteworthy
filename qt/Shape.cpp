

#include <unordered_map>
#include "Shape.hpp"

#ifdef NOTEWORTHY_QT
const std::unordered_map<Shape::ShapeType, QString> Shape::shapeSvgPaths {
    {Shape::SQUARE, ":/png/png/square.svg"},
    {Shape::CIRCLE, ":/png/png/circle.svg"},
    {Shape::RECTANGLE, ":/png/png/rectangle.svg"},
    {Shape::ELLIPSE, ":/png/png/ellipse.svg"},
    {Shape::STAR, ":/png/png/star.svg"},
    {Shape::TWOD_PLOT, ":/png/png/2d_plot.svg"},
    {Shape::THREED_PLOT, ":/png/png/3d_plot.svg"},
    {Shape::ARROW, ":/png/png/arrow.svg"},
    {Shape::LINE, ":/png/png/line.svg"}
};
#endif
