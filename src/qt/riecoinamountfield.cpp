// Copyright (c) The Riecoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/riecoinamountfield.h>

#include <qt/guiconstants.h>
#include <qt/guiutil.h>
#include <qt/qvaluecombobox.h>

#include <QApplication>
#include <QAbstractSpinBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QVariant>

#include <cassert>

RiecoinAmountField::RiecoinAmountField(QWidget *parent): QLineEdit(parent)
{
    QDoubleValidator *amountValidator = new QDoubleValidator(0, 5e6, 8, this);
    QLocale amountLocale(QLocale::C);
    amountLocale.setNumberOptions(QLocale::RejectGroupSeparator);
    amountValidator->setLocale(amountLocale);
    setValidator(amountValidator);

    installEventFilter(this);
    setFixedWidth(144);
    setAlignment(Qt::AlignRight);

    connect(this, &QLineEdit::textEdited, this, &QLineEdit::textChanged);
    validate();
}

bool RiecoinAmountField::validate()
{
    bool valid{false};
    if (m_optional && text().isEmpty())
        valid = true;
    else {
        CAmount amount(value(&valid));
        if (amount < m_min_amount || amount > m_max_amount)
            valid = false;
    }
    setStyleSheet(valid ? "" : STYLE_INVALID);
    return valid;
}

void RiecoinAmountField::setValue(const CAmount& value) {
    setText(GUIUtil::formatAmount(value, false, GUIUtil::SeparatorStyle::NEVER));
    validate();
    Q_EMIT valueChanged();
}

CAmount RiecoinAmountField::parse(const QString &text, bool *valid_out) const
{
    CAmount val = 0;
    bool valid = ParseFixedPoint(text.toStdString(), 8, &val);
    if (valid) {
        if (val < m_min_amount || val > m_max_amount)
            valid = false;
    }
    if (valid_out)
        *valid_out = valid;
    return valid ? val : 0;
}

bool RiecoinAmountField::event(QEvent *event)
{
    if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Comma) {
            // Translate a comma into a period
            QKeyEvent periodKeyEvent(event->type(), Qt::Key_Period, keyEvent->modifiers(), ".", keyEvent->isAutoRepeat(), keyEvent->count());
            return QLineEdit::event(&periodKeyEvent);
        }
        validate();
    }
    return QLineEdit::event(event);
}
