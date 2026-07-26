#include "ToolbarBuilder.h"
#include <QColorDialog>
#include <RichTextEdit.h>

ToolbarBuilder::ToolbarBuilder(QToolBar* toolbar, DemoWindow* pWindow)
    : _pToolbar(toolbar)
    , _pWindow(pWindow)
{
}

ToolbarBuilder::~ToolbarBuilder() = default;

void ToolbarBuilder::Build() {
    _pToolbar->setIconSize({20, 20});

    BuildFontControls();
    _pToolbar->addSeparator();
    BuildUndoRedoControls();
    _pToolbar->addSeparator();
    BuildFormatControls();
    _pToolbar->addSeparator();
    BuildColorControls();
    _pToolbar->addSeparator();
    BuildAlignmentControls();
    _pToolbar->addSeparator();
    BuildIndentControls();
    _pToolbar->addSeparator();
    BuildSpecialCharControls();
    _pToolbar->addSeparator();
    BuildWordWrapControl();
    _pToolbar->addSeparator();
    BuildProtectionControl();
}

void ToolbarBuilder::BuildFontControls() {
    _pFontComboBox = new QFontComboBox(_pToolbar);
    _pToolbar->addWidget(_pFontComboBox);
    QObject::connect(_pFontComboBox, &QFontComboBox::currentFontChanged, _pWindow, [this](const QFont& font) {
        QTextCharFormat fmt;
        fmt.setFontFamilies({font.family()});
        _pWindow->MergeFormatOnSelection(fmt);
    });

    _pFontSize = new QSpinBox(_pToolbar);
    _pFontSize->setRange(6, 96);
    _pFontSize->setValue(12);
    _pFontSize->setFixedWidth(56);
    _pToolbar->addWidget(_pFontSize);
    QObject::connect(_pFontSize, qOverload<int>(&QSpinBox::valueChanged), _pWindow,
        [this](int size) {
            QTextCharFormat fmt;
            fmt.setFontPointSize(static_cast<double>(size));
            _pWindow->MergeFormatOnSelection(fmt);
        });
}

void ToolbarBuilder::BuildUndoRedoControls() {
    QAction* pUndo = _pToolbar->addAction(QIcon::fromTheme("edit-undo"), "Undo");
    pUndo->setShortcut(Qt::CTRL | Qt::Key_Z);
    QObject::connect(pUndo, &QAction::triggered, _pWindow->Editor(), &QTextEdit::undo);

    QAction* pRedo = _pToolbar->addAction(QIcon::fromTheme("edit-redo"), "Redo");
    pRedo->setShortcut(Qt::CTRL | Qt::Key_Y);
    QObject::connect(pRedo, &QAction::triggered, _pWindow->Editor(), &QTextEdit::redo);
}

void ToolbarBuilder::BuildFormatControls() {
    _pBold = _pToolbar->addAction(QIcon::fromTheme("format-text-bold"), "Bold");
    _pBold->setCheckable(true);
    _pBold->setShortcut(Qt::CTRL | Qt::Key_B);
    QObject::connect(_pBold, &QAction::toggled, _pWindow, &DemoWindow::ToggleBold);

    _pItalic = _pToolbar->addAction(QIcon::fromTheme("format-text-italic"), "Italic");
    _pItalic->setCheckable(true);
    _pItalic->setShortcut(Qt::CTRL | Qt::Key_I);
    QObject::connect(_pItalic, &QAction::toggled, _pWindow, &DemoWindow::ToggleItalic);

    _pUnderline = _pToolbar->addAction(QIcon::fromTheme("format-text-underline"), "Underline");
    _pUnderline->setCheckable(true);
    _pUnderline->setShortcut(Qt::CTRL | Qt::Key_U);
    QObject::connect(_pUnderline, &QAction::toggled, _pWindow, &DemoWindow::ToggleUnderline);

    _pStrikethrough = _pToolbar->addAction(QIcon::fromTheme("format-text-strikethrough"), "Strikethrough");
    _pStrikethrough->setCheckable(true);
    QObject::connect(_pStrikethrough, &QAction::toggled, _pWindow, &DemoWindow::ToggleStrikethrough);

    _pSuperscript = _pToolbar->addAction("Superscript");
    _pSuperscript->setCheckable(true);
    QObject::connect(_pSuperscript, &QAction::toggled, _pWindow, &DemoWindow::ToggleSuperscript);

    _pSubscript = _pToolbar->addAction("Subscript");
    _pSubscript->setCheckable(true);
    QObject::connect(_pSubscript, &QAction::toggled, _pWindow, &DemoWindow::ToggleSubscript);
}

void ToolbarBuilder::BuildColorActions(QMenu* pMenu, bool isTextColor) {
    const std::vector<std::pair<QString, QString>> colors = {
        {"Black", "#000000"}, {"White", "#FFFFFF"},
        {"Red", "#FF0000"}, {"Green", "#008000"},
        {"Blue", "#0000FF"}, {"Yellow", "#FFFF00"},
        {"Cyan", "#00FFFF"}, {"Magenta", "#FF00FF"},
    };

    for (const auto& [name, hex] : colors) {
        QAction* pAction = pMenu->addAction(name);
        QObject::connect(pAction, &QAction::triggered, _pWindow, [this, hex, isTextColor] {
            QColor color(hex);
            QTextCharFormat fmt;
            if (isTextColor) {
                fmt.setForeground(color);
            } else {
                fmt.setBackground(color);
            }
            _pWindow->MergeFormatOnSelection(fmt);
        });
    }

    QAction* pCustom = pMenu->addAction("Custom...");
    QObject::connect(pCustom, &QAction::triggered, _pWindow, [this, isTextColor] {
        QColor color = QColorDialog::getColor(isTextColor ? Qt::black : Qt::yellow, _pWindow);
        if (color.isValid()) {
            QTextCharFormat fmt;
            if (isTextColor) {
                fmt.setForeground(color);
            } else {
                fmt.setBackground(color);
            }
            _pWindow->MergeFormatOnSelection(fmt);
        }
    });
}

void ToolbarBuilder::BuildColorControls() {
    _pTextColorBtn = new QToolButton(_pToolbar);
    _pTextColorBtn->setText("A");
    _pTextColorBtn->setPopupMode(QToolButton::InstantPopup);
    _pTextColorMenu = new QMenu(_pTextColorBtn);
    _pTextColorBtn->setMenu(_pTextColorMenu);
    _pTextColorBtn->setAutoRaise(true);
    _pToolbar->addWidget(_pTextColorBtn);
    BuildColorActions(_pTextColorMenu, true);

    _pBgColorBtn = new QToolButton(_pToolbar);
    _pBgColorBtn->setText("A");
    _pBgColorBtn->setStyleSheet("background-color: #FF0; padding: 2px; border: 1px solid #999; border-radius: 2px;");
    _pBgColorBtn->setPopupMode(QToolButton::InstantPopup);
    _pBgColorMenu = new QMenu(_pBgColorBtn);
    _pBgColorBtn->setMenu(_pBgColorMenu);
    _pBgColorBtn->setAutoRaise(true);
    _pToolbar->addWidget(_pBgColorBtn);
    BuildColorActions(_pBgColorMenu, false);
}

void ToolbarBuilder::BuildAlignmentControls() {
    _pAlignLeft = _pToolbar->addAction(QIcon::fromTheme("format-justify-left"), "Left");
    _pAlignLeft->setCheckable(true);
    _pAlignLeft->setChecked(true);
    QObject::connect(_pAlignLeft, &QAction::triggered, _pWindow, [this] { _pWindow->ApplyAlignment(Qt::AlignLeft); });

    _pAlignCenter = _pToolbar->addAction(QIcon::fromTheme("format-justify-center"), "Center");
    _pAlignCenter->setCheckable(true);
    QObject::connect(_pAlignCenter, &QAction::triggered, _pWindow, [this] { _pWindow->ApplyAlignment(Qt::AlignHCenter); });

    _pAlignRight = _pToolbar->addAction(QIcon::fromTheme("format-justify-right"), "Right");
    _pAlignRight->setCheckable(true);
    QObject::connect(_pAlignRight, &QAction::triggered, _pWindow, [this] { _pWindow->ApplyAlignment(Qt::AlignRight); });

    _pAlignJustify = _pToolbar->addAction(QIcon::fromTheme("format-justify-fill"), "Justify");
    _pAlignJustify->setCheckable(true);
    QObject::connect(_pAlignJustify, &QAction::triggered, _pWindow, [this] { _pWindow->ApplyAlignment(Qt::AlignJustify); });
}

void ToolbarBuilder::BuildIndentControls() {
    _pIndentDecrease = _pToolbar->addAction(QIcon::fromTheme("list-indent-less"), "Decrease indent");
    QObject::connect(_pIndentDecrease, &QAction::triggered, _pWindow, [this] { _pWindow->ApplyIndent(-10); });

    _pIndentIncrease = _pToolbar->addAction(QIcon::fromTheme("list-indent-more"), "Increase indent");
    QObject::connect(_pIndentIncrease, &QAction::triggered, _pWindow, [this] { _pWindow->ApplyIndent(10); });
}

void ToolbarBuilder::BuildSpecialCharControls() {
    _pSpecialChar = new QToolButton(_pToolbar);
    _pSpecialChar->setText("\u2022");
    _pSpecialChar->setPopupMode(QToolButton::InstantPopup);
    _pSpecialCharMenu = new QMenu(_pSpecialChar);
    _pSpecialChar->setMenu(_pSpecialCharMenu);
    _pSpecialChar->setAutoRaise(true);
    _pToolbar->addWidget(_pSpecialChar);

    auto* pBullet = _pSpecialCharMenu->addAction("\u2022 Bullet");
    QObject::connect(pBullet, &QAction::triggered, _pWindow, [this] { _pWindow->InsertSpecialChar(QChar(0x2022)); });

    auto* pEmdash = _pSpecialCharMenu->addAction("\u2014 Em dash");
    QObject::connect(pEmdash, &QAction::triggered, _pWindow, [this] { _pWindow->InsertSpecialChar(QChar(0x2014)); });

    auto* pendash = _pSpecialCharMenu->addAction("\u2013 En dash");
    QObject::connect(pendash, &QAction::triggered, _pWindow, [this] { _pWindow->InsertSpecialChar(QChar(0x2013)); });

    auto* plquote = _pSpecialCharMenu->addAction("\u2018 Left quote");
    QObject::connect(plquote, &QAction::triggered, _pWindow, [this] { _pWindow->InsertSpecialChar(QChar(0x2018)); });

    auto* prquote = _pSpecialCharMenu->addAction("\u2019 Right quote");
    QObject::connect(prquote, &QAction::triggered, _pWindow, [this] { _pWindow->InsertSpecialChar(QChar(0x2019)); });

    auto* pdblquote = _pSpecialCharMenu->addAction("\u201C Left double quote");
    QObject::connect(pdblquote, &QAction::triggered, _pWindow, [this] { _pWindow->InsertSpecialChar(QChar(0x201C)); });

    auto* prdblquote = _pSpecialCharMenu->addAction("\u201D Right double quote");
    QObject::connect(prdblquote, &QAction::triggered, _pWindow, [this] { _pWindow->InsertSpecialChar(QChar(0x201D)); });
}

void ToolbarBuilder::BuildWordWrapControl() {
    _pWordWrap = new QAction(QIcon::fromTheme("view-wrap-text"), "Word Wrap");
    _pWordWrap->setCheckable(true);
    _pWordWrap->setChecked(false);
    QObject::connect(_pWordWrap, &QAction::toggled, _pWindow, &DemoWindow::ToggleWordWrap);
}

void ToolbarBuilder::BuildProtectionControl() {
    QAction* setProt = _pToolbar->addAction(QIcon::fromTheme("object-lock"), "Set protection...");
    QObject::connect(setProt, &QAction::triggered, _pWindow, &DemoWindow::SetProtection);
}
