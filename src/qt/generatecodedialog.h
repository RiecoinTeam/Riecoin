// Copyright (c) 2013-2021 The Riecoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_GENERATECODEDIALOG_H
#define BITCOIN_QT_GENERATECODEDIALOG_H

#include <QDialog>

class PlatformStyle;
class WalletModel;

namespace Ui {
    class GenerateCodeDialog;
}

class GenerateCodeDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GenerateCodeDialog(const PlatformStyle *platformStyle, QWidget *parent);
    ~GenerateCodeDialog();
    void setModel(WalletModel *model);

private:
    Ui::GenerateCodeDialog *ui;
    WalletModel *model;
    const PlatformStyle *platformStyle;
    QTimer *timer;

private Q_SLOTS:
    void on_addressBookButton_clicked();
    void on_copyCodeButton_clicked();
    void refresh();
};

#endif // BITCOIN_QT_GENERATECODEDIALOG_H
