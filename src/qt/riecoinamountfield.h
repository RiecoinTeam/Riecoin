// Copyright (c) The Riecoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef RIECOIN_QT_RIECOINAMOUNTFIELD_H
#define RIECOIN_QT_RIECOINAMOUNTFIELD_H

#include <qt/guiutil.h>

#include <QWidget>
#include <QLineEdit>
#include <QValidator>

/** QSpinBox that uses fixed-point numbers internally and uses our own formatting/parsing functions. */
class RiecoinAmountField: public QLineEdit
{
    Q_OBJECT

public:
    explicit RiecoinAmountField(QWidget *parent);

    void SetOptional(const bool optional) {m_optional = optional; validate();}
    void SetMinValue(const CAmount& value) {m_min_amount = value; validate();}
    void SetMaxValue(const CAmount& value) {m_max_amount = value; validate();}

    bool validate();
    CAmount value(bool *valid_out=nullptr) const {return parse(text(), valid_out);}
    void setValue(const CAmount& value);

private:
    bool m_optional{false};
    CAmount m_min_amount{CAmount(0)};
    CAmount m_max_amount{CAmount(500000000000000)};

    CAmount parse(const QString &text, bool *valid_out=nullptr) const;

protected:
    bool event(QEvent *event) override;

Q_SIGNALS:
    void valueChanged();
};

#endif // RIECOIN_QT_RIECOINAMOUNTFIELD_H
