#include "app.h"

#include <QApplication>
#include <QLocale>
#include <QTranslator>

int main(int argc, char* argv[])
{
	QApplication a(argc, argv);

	QTranslator translator;
	const QStringList ui_languages = QLocale::system().uiLanguages();
	for (const QString& locale : ui_languages)
	{
		if (const QString base_name = "lambda-reader_" + QLocale(locale).name(); translator.load(":/i18n/" + base_name))
		{
			QApplication::installTranslator(&translator);
			break;
		}
	}
	app w;
	w.show();
	return QApplication::exec();
}
