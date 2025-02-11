#ifndef LAMBDA_READER_APP_H
#define LAMBDA_READER_APP_H

#include <functional>

#include <QMainWindow>
#include <QPdfDocument>
#include <QPdfView>
#include <QPdfPageNavigator>
#include <QPdfBookmarkModel>
#include <QMenuBar>
#include <QFileDialog>
#include <QMouseEvent>
#include <QMessageBox>
#include <QScrollBar>
#include <QListWidget>
#include <QVBoxLayout>
#include <QStackedWidget>
#include <QString>
#include <QSpinBox>

struct bookmark
{
	int page_number;
	QString title;
};

class app final : public QMainWindow
{
	Q_OBJECT

public:
	explicit app(QWidget* parent = nullptr);
	~app() override;

private slots:
	void open_file();
	void update_page_number(int page) const;
	void jump_to_page(int page) const;
	void update_zoom_factor(const QString& zoom) const;

protected:
	bool eventFilter(QObject* watched, QEvent* event) override;

private:
	void create_menus();
	void setup_sidebar();
	void setup_thumbnail_view();
	void setup_bookmark_view();
	void setup_floating_toolbar();
	void switch_to_thumbnails() const;
	void switch_to_bookmarks() const;
	void bookmark_selected(const QListWidgetItem* item) const;

	QPdfView* pdf_view_;
	QPdfView* thumbnail_view_;
	QStackedWidget* sidebar_stack_;
	QWidget* sidebar_;
	QListWidget* bookmark_list_;
	QPdfDocument* current_document_;
	QPdfBookmarkModel* bookmark_model_;
	QList<bookmark> bookmarks_;

	// Floating toolbar components
	QToolBar* floating_toolbar_;
	QSpinBox* page_spin_;
	QLabel* total_pages_label_;
	QComboBox* zoom_combo_;
};

#endif // LAMBDA_READER_APP_H
