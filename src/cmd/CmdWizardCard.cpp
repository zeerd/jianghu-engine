#include "CmdWizard.h"

#include "../db/Sql.h"

int CCmdWizard::create_card(CChar* me, int32_t target, char* arg)
{
    ::create_card(me);
    return 1;
}
