#include "squid.h"
#include "acl/Checklist.h"
#include "acl/AtBumpStep.h"
#include "acl/AtBumpStepData.h"
#include "client_side.h"
#include "ssl/ServerBump.h"
//#include "ssl/support.h"

int
ACLAtStepStrategy::match (ACLData<Ssl::BumpStep> * &data, ACLFilledChecklist *checklist, ACLFlags &)
{
    if (checklist->conn() != NULL) {
        if (Ssl::ServerBump *bump = checklist->conn()->serverBump())
            return data->match(bump->step);
        else
            return data->match(Ssl::bumpStep1);
    }
    return 0;
}

ACLAtStepStrategy *
ACLAtStepStrategy::Instance()
{
    return &Instance_;
}

ACLAtStepStrategy ACLAtStepStrategy::Instance_;
