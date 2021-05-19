#pragma once
#include <QIcon>
#include <QtSvg/QSvgRenderer>
#include <QByteArray>
#include <QPainter>
namespace translateLocally {
    namespace logo {
        constexpr static const char logoSVG[] = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\
<!-- Generator: Adobe Illustrator 23.1.0, SVG Export Plug-In . SVG Version: 6.00 Build 0)  -->\
<svg version=\"1.1\" id=\"Layer_1\" xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" x=\"0px\" y=\"0px\"\
	 viewBox=\"0 0 800 800\" style=\"enable-background:new 0 0 800 800;\" xml:space=\"preserve\">\
<style type=\"text/css\">\
	.st0{fill:#BBBDBF;}\
	.st1{fill:#FFFFFF;}\
	.st2{fill:#00ADEE;}\
</style>\
<g>\
	<path class=\"st0\" d=\"M262.77,121.95c-130.3,0-235.93,105.63-235.93,235.93c0,70.93,31.31,134.54,80.85,177.79\
		c-2.21,4.61-27.69,54.58-86.65,40.67c95.06,57.39,165.97,10.53,171.35,6.79c22.23,6.94,45.87,10.68,70.39,10.68\
		c130.3,0,235.93-105.63,235.93-235.93S393.07,121.95,262.77,121.95z\"/>\
	<g>\
		<path class=\"st1\" d=\"M392.7,471.49l-16.6,24.44c-46.88-17.2-86.46-38.53-118.75-63.98c-34.91,27.66-75.39,48.23-121.47,61.71\
			l-15.39-24.59c49.49-15.89,87.72-34.55,114.68-55.98c-23.74-24.54-43.71-58.04-59.9-100.49l26.56-10.71\
			c15.09,40.94,33.4,71.72,54.92,92.34c22.43-21.83,40.39-54.57,53.87-98.23H122.61v-27.61h118.15\
			c-6.14-13.68-13.48-26.66-22.03-38.93l28.82-9.2c10.76,17.6,18.91,33.65,24.44,48.13h118.9V296H341.1\
			c-15.49,50.6-36.16,89.63-62.02,117.09C305.54,434.32,343.41,453.78,392.7,471.49z\"/>\
	</g>\
	<path class=\"st2\" d=\"M537.23,195.31c130.3,0,235.93,105.63,235.93,235.93c0,70.93-31.31,134.54-80.85,177.79\
		c2.21,4.61,27.69,54.58,86.65,40.67c-95.06,57.39-165.97,10.53-171.35,6.79c-22.23,6.94-45.87,10.68-70.39,10.68\
		c-130.3,0-235.93-105.63-235.93-235.93S406.93,195.31,537.23,195.31z\"/>\
	<path class=\"st1\" d=\"M610.55,559.5h35.24l-93.98-254.94H521.3L427.32,559.5h35.24l20.1-57.86h107.79L610.55,559.5z M493.32,470.95\
		l43.24-124.49l43.24,124.49H493.32z\"/>\
</g>\
</svg>";

        static QIcon getLogoFromSVG() {
            QByteArray infile(logoSVG);
            QSvgRenderer rr(infile);
            QImage image(1024, 751, QImage::Format_ARGB32);
            QPainter painter(&image);
            image.fill(Qt::transparent);
            rr.render(&painter);
            return QIcon(QPixmap::fromImage(image));
        }
    } // namespace logo
} // namespace translateLocally

