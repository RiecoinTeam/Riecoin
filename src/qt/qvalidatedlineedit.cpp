// Copyright (c) The Bitcoin Core developers
// Copyright (c) The Riecoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/qvalidatedlineedit.h>

#include <qt/riecoinaddressvalidator.h>
#include <qt/guiconstants.h>

QValidatedLineEdit::QValidatedLineEdit(QWidget* parent) : QLineEdit(parent)
{
    connect(this, &QValidatedLineEdit::textChanged, this, &QValidatedLineEdit::checkValidity);
    checkValidity();
}

void QValidatedLineEdit::setText(const QString& text)
{
    QLineEdit::setText(text);
    checkValidity();
}

void QValidatedLineEdit::setValid(bool _valid)
{
    if (_valid == this->valid)
        return;

    setStyleSheet(_valid ? "" : "QValidatedLineEdit { " STYLE_INVALID "}");
    this->valid = _valid;
}

void QValidatedLineEdit::focusInEvent(QFocusEvent *evt)
{
    QLineEdit::focusInEvent(evt);
}

void QValidatedLineEdit::focusOutEvent(QFocusEvent *evt)
{
    checkValidity();
    QLineEdit::focusOutEvent(evt);
}

void QValidatedLineEdit::clear()
{
    QLineEdit::clear();
    checkValidity();
}

void QValidatedLineEdit::setEnabled(bool enabled)
{
    if (!enabled) // A disabled QValidatedLineEdit should be marked valid
        setValid(true);
    else // Recheck validity when QValidatedLineEdit gets enabled
        checkValidity();
    QLineEdit::setEnabled(enabled);
}

void QValidatedLineEdit::checkValidity()
{
    if (text().isEmpty())
        setValid(optional);
    else if (hasAcceptableInput()) {
        setValid(true);

        // Check contents on focus out
        if (checkValidator) {
            QString address = text();
            int pos = 0;
            if (checkValidator->validate(address, pos) == QValidator::Acceptable)
                setValid(true);
            else
                setValid(false);
        }
    }
    else
        setValid(false);

    Q_EMIT validationDidChange(this);
}

void QValidatedLineEdit::setCheckValidator(const QValidator *v)
{
    checkValidator = v;
    checkValidity();
}

bool QValidatedLineEdit::isValid()
{
    // use checkValidator in case the QValidatedLineEdit is disabled
    if (checkValidator)
    {
        QString address = text();
        int pos = 0;
        if (checkValidator->validate(address, pos) == QValidator::Acceptable)
            return true;
    }

    return valid;
}
