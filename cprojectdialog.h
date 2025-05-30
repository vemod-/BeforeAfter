#ifndef CPROJECTDIALOG_H
#define CPROJECTDIALOG_H

#include <QDialog>
#include <QListView>

namespace Ui {
class CProjectDialog;
}

class CProjectDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CProjectDialog(QWidget *parent = nullptr);
    ~CProjectDialog();
    void exec(const QStringList& allProjects, QStringList& selectedProjects, QString& title);
private:
    Ui::CProjectDialog *ui;
};

#endif // CPROJECTDIALOG_H
