#include <QCoreApplication>
#include "src/core/MeshClient.h"
#include "src/ui/CLI/CommandLineInterface.h"

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    // Set application metadata
    QCoreApplication::setApplicationName("MeshCoreQt");
    QCoreApplication::setApplicationVersion("1.0.0");
    QCoreApplication::setOrganizationName("MeshCore");

    // Create MeshCore client
    MeshCore::MeshClient client;

    // Create and start CLI
    MeshCore::CommandLineInterface cli(&client);
    cli.start();

    return app.exec();
}
