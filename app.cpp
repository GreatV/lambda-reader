#include "app.h"

#include <QHBoxLayout>
#include <QMessageBox>
#include <QLabel>
#include <QToolBar>
#include <QComboBox>

app::app(QWidget* parent)
	: QMainWindow(parent),
	  pdf_view_(nullptr),
	  thumbnail_view_(nullptr),
	  sidebar_stack_(nullptr),
	  sidebar_(nullptr),
	  bookmark_list_(nullptr),
	  current_document_(nullptr),
	  bookmark_model_(nullptr),
	  floating_toolbar_(nullptr),
	  page_spin_(nullptr),
	  total_pages_label_(nullptr),
	  zoom_combo_(nullptr)
{
	setWindowTitle("Lambda Reader");

	// Create central widget with horizontal layout
	auto* central_widget = new QWidget(this);
	auto* main_layout = new QHBoxLayout(central_widget);
	main_layout->setContentsMargins(0, 0, 0, 0);
	main_layout->setSpacing(0);
	setCentralWidget(central_widget);

	// Create views first
	pdf_view_ = new QPdfView(this);
	pdf_view_->setPageMode(QPdfView::PageMode::MultiPage);

	// Create bookmark model
	bookmark_model_ = new QPdfBookmarkModel(this);

	// Setup sidebar (this will create thumbnail_view_)
	setup_sidebar();

	// Add widgets to layout
	main_layout->addWidget(sidebar_);
	main_layout->addWidget(pdf_view_);

	// Setup floating toolbar
	setup_floating_toolbar();

	// Create menus
	create_menus();

	// Show maximized on startup
	showMaximized();
}

app::~app()
= default;

void app::create_menus()
{
	QMenu* file_menu = menuBar()->addMenu(tr("&File"));

	QAction* open_act = file_menu->addAction(tr("&Open..."), this, &app::open_file);
	open_act->setShortcuts(QKeySequence::Open);
	open_act->setStatusTip(tr("Open a PDF file"));

	file_menu->addSeparator();

	QAction* exit_act = file_menu->addAction(tr("E&xit"), this, &QWidget::close);
	exit_act->setShortcuts(QKeySequence::Quit);
	exit_act->setStatusTip(tr("Exit the application"));
}

void app::open_file()
{
	const QString file_name = QFileDialog::getOpenFileName(
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
	if (const QPdfDocument::Error error = current_document_->load(file_name); error != QPdfDocument::Error::None)
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
		std::function<void(const QModelIndex&)> load_bookmarks;
		load_bookmarks = [this, &load_bookmarks](const QModelIndex& parent)
		{
			if (!bookmark_model_ || !bookmark_list_)
			{
				return;
			}

			const int row_count = bookmark_model_->rowCount(parent);
			for (int i = 0; i < row_count; ++i)
			{
				QModelIndex idx = bookmark_model_->index(i, 0, parent);
				const int page = bookmark_model_->data(idx, static_cast<int>(QPdfBookmarkModel::Role::Page)).toInt();
				QString title = bookmark_model_->data(idx, static_cast<int>(QPdfBookmarkModel::Role::Title)).toString();

				bookmark bookmark{page, title};
				bookmarks_.append(bookmark);

				auto* list_item = new QListWidgetItem(title);
				list_item->setData(Qt::UserRole, page);
				bookmark_list_->addItem(list_item);

				// Recursively load child bookmarks
				load_bookmarks(idx);
			}
		};

		load_bookmarks(QModelIndex()); // Start from root
	}
}

bool app::eventFilter(QObject* watched, QEvent* event)
{
	if (!thumbnail_view_ || !current_document_ || !watched || !event)
	{
		return QMainWindow::eventFilter(watched, event);
	}

	if (watched == thumbnail_view_->viewport())
	{
		if (event->type() == QEvent::MouseButtonRelease)
		{
			const auto* mouse_event = dynamic_cast<QMouseEvent*>(event);
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
			const auto* v_scroll_bar = thumbnail_view_->verticalScrollBar();
			if (!v_scroll_bar)
			{
				return QMainWindow::eventFilter(watched, event);
			}

			const int scroll_pos = v_scroll_bar->value();
			const int viewport_height = thumbnail_view_->viewport()->height();

			// Calculate relative position in the document
			const double relative_pos = (scroll_pos + mouse_event->pos().y()) /
				static_cast<double>(v_scroll_bar->maximum() + viewport_height);

			// Calculate approximate page number
			const int total_pages = current_document_->pageCount();

			// Update main view to show the clicked page
			if (const int page = qBound(0, static_cast<int>(relative_pos * total_pages), total_pages - 1); page >= 0 &&
				pdf_view_ && pdf_view_->pageNavigator())
			{
				pdf_view_->pageNavigator()->jump(page, QPointF(0.5, 0.5), 0);
				return true;
			}
		}
	}
	return QMainWindow::eventFilter(watched, event);
}

void app::setup_sidebar()
{
	// Create sidebar widget
	sidebar_ = new QWidget(this);

	sidebar_->setFixedWidth(250);
	auto* sidebar_layout = new QVBoxLayout(sidebar_);
	sidebar_layout->setContentsMargins(0, 0, 0, 0);
	sidebar_layout->setSpacing(0);

	// Create toolbar for view switching
	auto* tool_bar = new QToolBar(sidebar_);
	tool_bar->setIconSize(QSize(24, 24));
	tool_bar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

	const QAction* thumbnail_action = tool_bar->addAction(
		QIcon(":/icons/layers.svg"), tr("Thumbnails"));
	const QAction* bookmark_action = tool_bar->addAction(
		QIcon(":/icons/bookmark.svg"), tr("Bookmarks"));

	tool_bar->setStyleSheet("QToolBar { border: none; border-bottom: 1px solid #ccc; }");

	connect(thumbnail_action, &QAction::triggered, this, &app::switch_to_thumbnails);
	connect(bookmark_action, &QAction::triggered, this, &app::switch_to_bookmarks);

	// Add toolbar to sidebar
	sidebar_layout->addWidget(tool_bar);

	// Create stacked widget for views
	sidebar_stack_ = new QStackedWidget(sidebar_);
	sidebar_layout->addWidget(sidebar_stack_);

	// Setup views
	setup_thumbnail_view();
	setup_bookmark_view();

	// Set initial view to thumbnails
	switch_to_thumbnails();
}

void app::setup_thumbnail_view()
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
	auto synchronize_page = [](const QPdfPageNavigator* source, QPdfPageNavigator* target)
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
		        [synchronize_page, this]()
		        {
			        synchronize_page(pdf_view_->pageNavigator(),
			                         thumbnail_view_->pageNavigator());
		        });

		connect(thumbnail_view_->pageNavigator(), &QPdfPageNavigator::currentPageChanged,
		        this,
		        [synchronize_page, this]()
		        {
			        synchronize_page(thumbnail_view_->pageNavigator(),
			                         pdf_view_->pageNavigator());
		        });
	}

	sidebar_stack_->addWidget(thumbnail_view_);
}

void app::setup_bookmark_view()
{
	bookmark_list_ = new QListWidget(sidebar_stack_);

	sidebar_stack_->addWidget(bookmark_list_);
	connect(bookmark_list_, &QListWidget::itemClicked, this, &app::bookmark_selected);
}

void app::switch_to_thumbnails() const
{
	if (sidebar_stack_ && thumbnail_view_)
	{
		sidebar_stack_->setCurrentWidget(thumbnail_view_);
	}
}

void app::switch_to_bookmarks() const
{
	if (sidebar_stack_ && bookmark_list_)
	{
		sidebar_stack_->setCurrentWidget(bookmark_list_);
	}
}

void app::bookmark_selected(const QListWidgetItem* item) const
{
	if (!item || !current_document_ || !pdf_view_ || !pdf_view_->pageNavigator())
	{
		return;
	}

	const int page = item->data(Qt::UserRole).toInt();
	pdf_view_->pageNavigator()->jump(page, QPointF(0.5, 0.5), 0); // Convert to 0-based for jump
}

void app::setup_floating_toolbar()
{
	floating_toolbar_ = new QToolBar(this);
	floating_toolbar_->setMovable(true);
	floating_toolbar_->setFloatable(true);

	// Add to bottom area and center it
	addToolBar(Qt::BottomToolBarArea, floating_toolbar_);

	// Create a spacer widget to push content to center
	auto* spacer = new QWidget(this);
	spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	floating_toolbar_->addWidget(spacer);

	// Previous page button
	QAction* prev_page = floating_toolbar_->addAction(tr("Previous"));
	prev_page->setShortcut(QKeySequence::MoveToPreviousPage);
	connect(prev_page, &QAction::triggered, this, [this]()
	{
		if (pdf_view_ && pdf_view_->pageNavigator())
		{
			pdf_view_->pageNavigator()->jump(pdf_view_->pageNavigator()->currentPage() - 1, QPointF(0.5, 0.5), 0);
		}
	});

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
	QAction* next_page = floating_toolbar_->addAction(tr("Next"));
	next_page->setShortcut(QKeySequence::MoveToNextPage);
	connect(next_page, &QAction::triggered, this, [this]()
	{
		if (pdf_view_ && pdf_view_->pageNavigator())
		{
			pdf_view_->pageNavigator()->jump(pdf_view_->pageNavigator()->currentPage() + 1, QPointF(0.5, 0.5), 0);
		}
	});

	floating_toolbar_->addSeparator();

	// Zoom combo box
	zoom_combo_ = new QComboBox(this);
	zoom_combo_->setEditable(true);
	zoom_combo_->addItems({"50%", "75%", "100%", "125%", "150%", "200%", "300%"});
	zoom_combo_->setCurrentText("100%");
	floating_toolbar_->addWidget(new QLabel(tr("Zoom:")));
	floating_toolbar_->addWidget(zoom_combo_);

	// Add another spacer at the end to ensure center alignment
	auto* spacer2 = new QWidget(this);
	spacer2->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	floating_toolbar_->addWidget(spacer2);

	// Connect signals
	if (pdf_view_ && pdf_view_->pageNavigator())
	{
		connect(pdf_view_->pageNavigator(), &QPdfPageNavigator::currentPageChanged,
		        this, &app::update_page_number);
	}
	connect(page_spin_, QOverload<int>::of(&QSpinBox::valueChanged),
	        this, &app::jump_to_page);
	connect(zoom_combo_, &QComboBox::currentTextChanged,
	        this, &app::update_zoom_factor);
}

void app::update_page_number(const int page) const
{
	if (page_spin_)
	{
		page_spin_->setValue(page + 1); // Convert from 0-based to 1-based
	}
}

void app::jump_to_page(const int page) const
{
	if (pdf_view_ && pdf_view_->pageNavigator())
	{
		pdf_view_->pageNavigator()->jump(page - 1, QPointF(0.5, 0.5), 0); // Convert from 1-based to 0-based
	}
}

void app::update_zoom_factor(const QString& zoom) const
{
	if (!pdf_view_)
	{
		return;
	}

	QString zoom_str = zoom;
	zoom_str.remove('%');
	bool ok;
	const double factor = zoom_str.toDouble(&ok);
	if (ok)
	{
		pdf_view_->setZoomFactor(factor / 100.0);
	}
}
