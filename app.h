#ifndef LAMBDA_READER_APP_H_
#define LAMBDA_READER_APP_H_

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
#include <QToolBar>
#include <QStackedWidget>
#include <QString>
#include <QSpinBox>
#include <QLabel>
#include <QComboBox>

struct Bookmark
{
  int page_number;
  QString title;
};

class App : public QMainWindow
{
  Q_OBJECT

public:
  explicit App(QWidget *parent = nullptr);
  ~App() override;

private slots:
  void OpenFile();
  void UpdatePageNumber(int page);
  void JumpToPage(int page);
  void UpdateZoomFactor(const QString &zoom);

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;

private:
  void CreateMenus();
  void SetupSidebar();
  void SetupThumbnailView();
  void SetupBookmarkView();
  void SetupFloatingToolbar();
  void SwitchToThumbnails();
  void SwitchToBookmarks();
  void BookmarkSelected(QListWidgetItem *item);

  QPdfView *pdf_view_;
  QPdfView *thumbnail_view_;
  QStackedWidget *sidebar_stack_;
  QWidget *sidebar_;
  QListWidget *bookmark_list_;
  QPdfDocument *current_document_;
  QPdfBookmarkModel *bookmark_model_;
  QList<Bookmark> bookmarks_;

  // Floating toolbar components
  QToolBar *floating_toolbar_;
  QSpinBox *page_spin_;
  QLabel *total_pages_label_;
  QComboBox *zoom_combo_;
};

#endif // LAMBDA_READER_APP_H_
