#include "app.h"

#include <QHBoxLayout>
#include <QMessageBox>

App::App(QWidget *parent)
    : QMainWindow(parent),
      current_document_(nullptr),
      bookmark_model_(nullptr),
      pdf_view_(nullptr),
      thumbnail_view_(nullptr),
      bookmark_list_(nullptr),
      sidebar_stack_(nullptr),
      sidebar_(nullptr),
      floating_toolbar_(nullptr),
      page_spin_(nullptr),
      total_pages_label_(nullptr),
      zoom_combo_(nullptr)
{
  setWindowTitle("Lambda Reader");

  // Create central widget with horizontal layout
  QWidget *central_widget = new QWidget(this);
  QHBoxLayout *main_layout = new QHBoxLayout(central_widget);
  main_layout->setContentsMargins(0, 0, 0, 0);
  main_layout->setSpacing(0);
  setCentralWidget(central_widget);

  // Create views first
  pdf_view_ = new QPdfView(this);
  if (!pdf_view_)
  {
    QMessageBox::critical(this, tr("Error"), tr("Failed to create PDF view"));
    return;
  }
  pdf_view_->setPageMode(QPdfView::PageMode::MultiPage);

  // Create bookmark model
  bookmark_model_ = new QPdfBookmarkModel(this);
  if (!bookmark_model_)
  {
    QMessageBox::critical(this, tr("Error"), tr("Failed to create bookmark model"));
    return;
  }

  // Setup sidebar (this will create thumbnail_view_)
  SetupSidebar();
  if (!sidebar_ || !thumbnail_view_)
  {
    QMessageBox::critical(this, tr("Error"), tr("Failed to create sidebar components"));
    return;
  }

  // Add widgets to layout
  main_layout->addWidget(sidebar_);
  main_layout->addWidget(pdf_view_);

  // Setup floating toolbar
  SetupFloatingToolbar();

  // Create menus
  CreateMenus();

  // Show maximized on startup
  showMaximized();
}

App::~App() {}

void App::CreateMenus()
{
  QMenu *file_menu = menuBar()->addMenu(tr("&File"));

  QAction *open_act = file_menu->addAction(tr("&Open..."), this, &App::OpenFile);
  open_act->setShortcuts(QKeySequence::Open);
  open_act->setStatusTip(tr("Open a PDF file"));

  file_menu->addSeparator();

  QAction *exit_act = file_menu->addAction(tr("E&xit"), this, &QWidget::close);
  exit_act->setShortcuts(QKeySequence::Quit);
  exit_act->setStatusTip(tr("Exit the application"));
}

void App::OpenFile()
{
  QString file_name = QFileDialog::getOpenFileName(
      this, tr("Open PDF File"), QString(),
      tr("PDF Files (*.pdf);;All Files (*)"));

  if (file_name.isEmpty())
  {
    return;
  }

  if (current_document_)
  {
    current_document_->deleteLater();
  }
  current_document_ = new QPdfDocument(this);

  // Load document and check for errors
  QPdfDocument::Error error = current_document_->load(file_name);
  if (error != QPdfDocument::Error::None)
  {
    QMessageBox::critical(
        this, tr("Error"),
        tr("Failed to load PDF file: %1").arg(file_name));
    current_document_->deleteLater();
    current_document_ = nullptr;
    return;
  }

  if (pdf_view_ && thumbnail_view_)
  {
    pdf_view_->setDocument(current_document_);
    thumbnail_view_->setDocument(current_document_);
  }

  // Update page spin box maximum and total pages label
  if (page_spin_ && total_pages_label_)
  {
    page_spin_->setMaximum(current_document_->pageCount());
    total_pages_label_->setText(tr(" / %1").arg(current_document_->pageCount()));
  }

  // Clear existing bookmarks
  bookmarks_.clear();
  if (bookmark_list_)
  {
    bookmark_list_->clear();
  }

  // Set document for bookmark model and try to load bookmarks
  if (bookmark_model_ && bookmark_list_)
  {
    bookmark_model_->setDocument(current_document_);

    // Recursively load bookmarks from the model
    std::function<void(const QModelIndex &)> load_bookmarks;
    load_bookmarks = [this, &load_bookmarks](const QModelIndex &parent)
    {
      if (!bookmark_model_ || !bookmark_list_)
      {
        return;
      }

      int row_count = bookmark_model_->rowCount(parent);
      for (int i = 0; i < row_count; ++i)
      {
        QModelIndex idx = bookmark_model_->index(i, 0, parent);
        int page = bookmark_model_->data(idx, Qt::UserRole + 1).toInt();
        QString title = bookmark_model_->data(idx, Qt::DisplayRole).toString();

        Bookmark bookmark{page - 1, title}; // Convert to 0-based page number
        bookmarks_.append(bookmark);

        QListWidgetItem *list_item = new QListWidgetItem(title);
        list_item->setData(Qt::UserRole, page - 1);
        bookmark_list_->addItem(list_item);

        // Recursively load child bookmarks
        load_bookmarks(idx);
      }
    };

    load_bookmarks(QModelIndex()); // Start from root
  }
}

bool App::eventFilter(QObject *watched, QEvent *event)
{
  if (!thumbnail_view_ || !current_document_ || !watched || !event)
  {
    return QMainWindow::eventFilter(watched, event);
  }

  if (watched == thumbnail_view_->viewport())
  {
    if (event->type() == QEvent::MouseButtonRelease)
    {
      QMouseEvent *mouse_event = static_cast<QMouseEvent *>(event);
      if (!mouse_event)
      {
        return QMainWindow::eventFilter(watched, event);
      }

      // Only handle left button clicks
      if (mouse_event->button() != Qt::LeftButton)
      {
        return QMainWindow::eventFilter(watched, event);
      }

      // Get the vertical scroll position and viewport height
      QScrollBar *v_scroll_bar = thumbnail_view_->verticalScrollBar();
      if (!v_scroll_bar)
      {
        return QMainWindow::eventFilter(watched, event);
      }

      int scroll_pos = v_scroll_bar->value();
      int viewport_height = thumbnail_view_->viewport()->height();

      // Calculate relative position in the document
      double relative_pos = (scroll_pos + mouse_event->pos().y()) /
                            static_cast<double>(v_scroll_bar->maximum() + viewport_height);

      // Calculate approximate page number
      int total_pages = current_document_->pageCount();
      int page = qBound(0, static_cast<int>(relative_pos * total_pages), total_pages - 1);

      // Update main view to show the clicked page
      if (page >= 0 && pdf_view_ && pdf_view_->pageNavigator())
      {
        pdf_view_->pageNavigator()->jump(page, QPointF(0.5, 0.5), 0);
        return true;
      }
    }
  }
  return QMainWindow::eventFilter(watched, event);
}

void App::SetupSidebar()
{
  // Create sidebar widget
  sidebar_ = new QWidget(this);

  sidebar_->setFixedWidth(250);
  QVBoxLayout *sidebar_layout = new QVBoxLayout(sidebar_);
  sidebar_layout->setContentsMargins(0, 0, 0, 0);
  sidebar_layout->setSpacing(0);

  // Create toolbar for view switching
  QToolBar *tool_bar = new QToolBar(sidebar_);
  tool_bar->setIconSize(QSize(24, 24));
  tool_bar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

  QAction *thumbnail_action = tool_bar->addAction(
      QIcon(":/icons/layers.svg"), tr("Thumbnails"));
  QAction *bookmark_action = tool_bar->addAction(
      QIcon(":/icons/bookmark.svg"), tr("Bookmarks"));

  tool_bar->setStyleSheet("QToolBar { border: none; border-bottom: 1px solid #ccc; }");

  connect(thumbnail_action, &QAction::triggered, this, &App::SwitchToThumbnails);
  connect(bookmark_action, &QAction::triggered, this, &App::SwitchToBookmarks);

  // Add toolbar to sidebar
  sidebar_layout->addWidget(tool_bar);

  // Create stacked widget for views
  sidebar_stack_ = new QStackedWidget(sidebar_);
  sidebar_layout->addWidget(sidebar_stack_);

  // Setup views
  SetupThumbnailView();
  SetupBookmarkView();

  // Set initial view to thumbnails
  SwitchToThumbnails();
}

void App::SetupThumbnailView()
{
  // Create thumbnail view with optimal settings
  thumbnail_view_ = new QPdfView(sidebar_stack_);

  thumbnail_view_->setMinimumWidth(200);
  thumbnail_view_->setPageMode(QPdfView::PageMode::MultiPage);
  thumbnail_view_->setZoomFactor(0.2);
  thumbnail_view_->setZoomMode(QPdfView::ZoomMode::FitInView);

  // Handle mouse events in thumbnail view
  thumbnail_view_->viewport()->installEventFilter(this);

  // Setup bidirectional page synchronization
  auto synchronize_page = [](QPdfPageNavigator *source, QPdfPageNavigator *target)
  {
    if (!source || !target)
    {
      return;
    }
    if (source->currentPage() != target->currentPage())
    {
      target->jump(source->currentPage(), QPointF(0.5, 0.5), 0);
    }
  };

  if (pdf_view_ && pdf_view_->pageNavigator() && thumbnail_view_->pageNavigator())
  {
    connect(pdf_view_->pageNavigator(), &QPdfPageNavigator::currentPageChanged, this,
            [=]()
            {
              synchronize_page(pdf_view_->pageNavigator(),
                               thumbnail_view_->pageNavigator());
            });

    connect(thumbnail_view_->pageNavigator(), &QPdfPageNavigator::currentPageChanged,
            this,
            [=]()
            {
              synchronize_page(thumbnail_view_->pageNavigator(),
                               pdf_view_->pageNavigator());
            });
  }

  sidebar_stack_->addWidget(thumbnail_view_);
}

void App::SetupBookmarkView()
{
  bookmark_list_ = new QListWidget(sidebar_stack_);

  sidebar_stack_->addWidget(bookmark_list_);
  connect(bookmark_list_, &QListWidget::itemClicked, this, &App::BookmarkSelected);
}

void App::SwitchToThumbnails()
{
  if (sidebar_stack_ && thumbnail_view_)
  {
    sidebar_stack_->setCurrentWidget(thumbnail_view_);
  }
}

void App::SwitchToBookmarks()
{
  if (sidebar_stack_ && bookmark_list_)
  {
    sidebar_stack_->setCurrentWidget(bookmark_list_);
  }
}

void App::BookmarkSelected(QListWidgetItem *item)
{
  if (!item || !current_document_ || !pdf_view_ || !pdf_view_->pageNavigator())
  {
    return;
  }

  int page = item->data(Qt::UserRole).toInt();
  pdf_view_->pageNavigator()->jump(page, QPointF(0.5, 0.5), 0);
}

void App::SetupFloatingToolbar()
{
  floating_toolbar_ = new QToolBar(this);
  floating_toolbar_->setMovable(true);
  floating_toolbar_->setFloatable(true);

  // Add to bottom area and center it
  addToolBar(Qt::BottomToolBarArea, floating_toolbar_);

  // Create a spacer widget to push content to center
  QWidget *spacer = new QWidget(this);
  spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  floating_toolbar_->addWidget(spacer);

  // Previous page button
  QAction *prev_page = floating_toolbar_->addAction(tr("Previous"));
  prev_page->setShortcut(QKeySequence::MoveToPreviousPage);
  connect(prev_page, &QAction::triggered, this, [this]()
          {
    if (pdf_view_ && pdf_view_->pageNavigator()) {
      pdf_view_->pageNavigator()->jump(pdf_view_->pageNavigator()->currentPage() - 1, QPointF(0.5, 0.5), 0);
    } });

  // Page number spin box
  page_spin_ = new QSpinBox(this);
  page_spin_->setMinimum(1);
  page_spin_->setMaximum(1);
  page_spin_->setFixedWidth(70);
  floating_toolbar_->addWidget(page_spin_);

  // Total pages label
  total_pages_label_ = new QLabel(this);
  total_pages_label_->setText(" / 1");
  floating_toolbar_->addWidget(total_pages_label_);

  // Next page button
  QAction *next_page = floating_toolbar_->addAction(tr("Next"));
  next_page->setShortcut(QKeySequence::MoveToNextPage);
  connect(next_page, &QAction::triggered, this, [this]()
          {
    if (pdf_view_ && pdf_view_->pageNavigator()) {
      pdf_view_->pageNavigator()->jump(pdf_view_->pageNavigator()->currentPage() + 1, QPointF(0.5, 0.5), 0);
    } });

  floating_toolbar_->addSeparator();

  // Zoom combo box
  zoom_combo_ = new QComboBox(this);
  zoom_combo_->setEditable(true);
  zoom_combo_->addItems({"50%", "75%", "100%", "125%", "150%", "200%", "300%"});
  zoom_combo_->setCurrentText("100%");
  floating_toolbar_->addWidget(new QLabel(tr("Zoom:")));
  floating_toolbar_->addWidget(zoom_combo_);

  // Add another spacer at the end to ensure center alignment
  QWidget *spacer2 = new QWidget(this);
  spacer2->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  floating_toolbar_->addWidget(spacer2);

  // Connect signals
  if (pdf_view_ && pdf_view_->pageNavigator())
  {
    connect(pdf_view_->pageNavigator(), &QPdfPageNavigator::currentPageChanged,
            this, &App::UpdatePageNumber);
  }
  connect(page_spin_, QOverload<int>::of(&QSpinBox::valueChanged),
          this, &App::JumpToPage);
  connect(zoom_combo_, &QComboBox::currentTextChanged,
          this, &App::UpdateZoomFactor);
}

void App::UpdatePageNumber(int page)
{
  if (page_spin_)
  {
    page_spin_->setValue(page + 1); // Convert from 0-based to 1-based
  }
}

void App::JumpToPage(int page)
{
  if (pdf_view_ && pdf_view_->pageNavigator())
  {
    pdf_view_->pageNavigator()->jump(page - 1, QPointF(0.5, 0.5), 0); // Convert from 1-based to 0-based
  }
}

void App::UpdateZoomFactor(const QString &zoom)
{
  if (!pdf_view_)
  {
    return;
  }

  QString zoom_str = zoom;
  zoom_str.remove('%');
  bool ok;
  double factor = zoom_str.toDouble(&ok);
  if (ok)
  {
    pdf_view_->setZoomFactor(factor / 100.0);
  }
}
