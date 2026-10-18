#include "stdafx.h"
#include "virtual_edit_session.h"
#include "physical_metadata_audit_ui.h"

DECLARE_COMPONENT_VERSION(
    "Metadata Isolation",
    "1.0.0-dev",
    "Routes compatible foobar2000 context commands to deduplicated physical files, "
    "audits embedded-Cue sources for missing or inconsistent physical core tags, "
    "and protects embedded-cuesheet subsongs from unsafe writes.\n\n"
    "Separates Physical and Virtual metadata layers; audit scans are read-only and "
    "unsupported or lossy Virtual operations remain fail-closed."
);

VALIDATE_COMPONENT_FILENAME("foo_metadata_isolation.dll");

namespace metadata_isolation {
namespace {

class session_shutdown : public initquit {
public:
    void on_init() override { cleanup_stale_virtual_edit_sessions(); }
    void on_quit() override {
        shutdown_physical_metadata_audit();
        shutdown_virtual_edit_session();
    }
};

initquit_factory_t<session_shutdown> g_session_shutdown_factory;

} // namespace
} // namespace metadata_isolation


