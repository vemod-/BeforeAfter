#include "cprojectdialog.h"
#include "ui_cprojectdialog.h"

CProjectDialog::CProjectDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::CProjectDialog)
{
    ui->setupUi(this);
}

CProjectDialog::~CProjectDialog()
{
    delete ui;
}

void CProjectDialog::exec(const QStringList &allProjects, QStringList &selectedProjects, QString& title) {
    ui->projectList->addItems(allProjects);

    QListWidgetItem* item = 0;
    for (int i = 0; i < ui->projectList->count(); ++i){
        item = ui->projectList->item(i);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
    }
    ui->titleEdit->setText(title);
    if (QDialog::exec()) {
        title = ui->titleEdit->text();
        QListWidgetItem* item = 0;
        for (int i = 0; i < ui->projectList->count(); ++i) {
            item = ui->projectList->item(i);
            if (item->checkState() == Qt::Checked) {
                selectedProjects.append(item->text());
            }
        }
    }
}
