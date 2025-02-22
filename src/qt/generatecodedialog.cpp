// Copyright (c) 2013-present The Riecoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/generatecodedialog.h>
#include <qt/forms/ui_generatecodedialog.h>

#include <qt/addressbookpage.h>
#include <qt/guiconstants.h>
#include <qt/guiutil.h>
#include <qt/platformstyle.h>
#include <qt/walletmodel.h>

#include <key_io.h>
#include <wallet/wallet.h>

#include <vector>

#include <QClipboard>
#include <QTimer>

GenerateCodeDialog::GenerateCodeDialog(const PlatformStyle *_platformStyle, QWidget *parent) : QDialog(parent), ui(new Ui::GenerateCodeDialog), model(nullptr), platformStyle(_platformStyle)
{
    ui->setupUi(this);
    ui->addressBookButton->setIcon(platformStyle->SingleColorIcon(":/icons/address-book"));
    ui->code->setWordWrapMode(QTextOption::WrapAnywhere);
    ui->copyCodeButton->setIcon(platformStyle->SingleColorIcon(":/icons/editcopy"));

    GUIUtil::setupAddressWidget(ui->addressIn, this);
    GUIUtil::handleCloseWindowShortcut(this);
    this->show();
    
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &GenerateCodeDialog::refresh);
    timer->setInterval(MODEL_UPDATE_DELAY);
    timer->start();
    lastUpdate = 0ULL;
    oldAddress = "";
}

GenerateCodeDialog::~GenerateCodeDialog()
{
    delete ui;
}

void GenerateCodeDialog::setModel(WalletModel *_model)
{
    this->model = _model;
}

void GenerateCodeDialog::on_addressBookButton_clicked()
{
    if (model && model->getAddressTableModel())
    {
        model->refresh();
        AddressBookPage dlg(platformStyle, AddressBookPage::ForSelection, AddressBookPage::ReceivingTab, this);
        dlg.setModel(model->getAddressTableModel());
        if (dlg.exec())
        {
            ui->addressIn->setText(dlg.getReturnValue());
        }
    }
    if (!model)
        return;
}

void GenerateCodeDialog::on_copyCodeButton_clicked()
{
    GUIUtil::setClipboard(ui->code->toPlainText());
}

constexpr uint64_t codeRefreshInterval(60ULL);
void GenerateCodeDialog::refresh()
{
    const uint64_t timestamp(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    const uint64_t codeTimestamp(timestamp - (timestamp % codeRefreshInterval));
    const std::string address(ui->addressIn->text().toStdString());
    CTxDestination destination = DecodeDestination(address);

    if (!IsValidDestination(destination)) {
        ui->statusLabel->setText(tr("Please enter a valid address."));
        ui->code->setText(QString::fromStdString("-"));
        return;
    }

    if (lastUpdate - codeTimestamp < 60ULL && address == oldAddress) {
        ui->statusLabel->setText(tr(("Refresh in " + std::to_string(codeRefreshInterval - (timestamp % codeRefreshInterval)) + " s").c_str()));
        return;
    }

    ui->codeQr->setQR("INVALID", "INVALID");
    lastUpdate = timestamp;
    oldAddress = address;

    WalletModel::UnlockContext ctx(model->requestUnlock());
    if (!ctx.isValid()) {
        ui->statusLabel->setText(tr("Wallet unlock was cancelled."));
        return;
    }

    std::string signature;
    SigningResult res = model->wallet().signMessage(MessageSignatureFormat::SIMPLE, std::to_string(codeTimestamp), destination, signature);

    QString error;
    switch (res) {
        case SigningResult::OK:
            error = tr("No error");
            break;
        case SigningResult::PRIVATE_KEY_NOT_AVAILABLE:
            error = tr("Private key for the entered address is not available.");
            break;
        case SigningResult::SIGNING_FAILED:
            error = tr("Message signing failed.");
            break;
        // no default case, so the compiler can warn about missing cases
    }

    if (res != SigningResult::OK) {
        ui->statusLabel->setText(error);
        ui->code->setText(QString::fromStdString("-"));
    }
    else {
        ui->code->setText(QString::fromStdString(signature));
        ui->codeQr->setQR(QString::fromStdString(signature), "Riecoin Authentication Code");
    }
}
