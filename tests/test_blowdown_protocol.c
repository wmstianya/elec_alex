#include "union_blowdown_core.h"
#include "slave_water_control.h"
#include <stdio.h>
#include <stdlib.h>

#define CHECK(c) do { if (!(c)) { printf("FAIL line %u: %s\n", (unsigned)__LINE__, #c); exit(1); } } while (0)

static UnionBlowdownState master;
static SlaveWaterInputs input;
static uint16_t status[4], command[4];
static unsigned opened;

static void advance(unsigned duration)
{
    unsigned n;
    for (n = 0; n < duration; ++n) {
        Slave_Water_Tick1ms();
        Slave_Water_Observe(7);
        input.valve_feedback = Slave_Water_ValveOpenAllowed() ? BD_VALVE_OPEN : BD_VALVE_CLOSED;
        Slave_Water_Service(&input);
        if (Slave_Water_ValveOpenAllowed()) ++opened;
    }
}

static void read_status(void)
{
    Slave_Water_Status(status);
    union_blowdown_observe(&master, 1, status, Slave_Water_NowMs());
    union_blowdown_step(&master, 1, 1, Slave_Water_NowMs());
}

static void deliver(void)
{
    CHECK(union_blowdown_command(&master, 1, command, Slave_Water_NowMs()));
    CHECK(Slave_Water_Command(command));
}

static void test_completed_session_rebind(void)
{
    const BlowdownConfig cfg = {10,100,30,200,20,100};
    Slave_Water_Init(); Slave_Water_SetBlowdownConfig(&cfg);
    CHECK(Slave_Water_BindPersistedSession(51));
    advance(WATER_RECOVERY_MS+20);
    union_blowdown_init(&master,51,Slave_Water_NowMs());
    union_blowdown_step(&master,1,1,Slave_Water_NowMs());
    CHECK(Slave_Water_Request(0,1));read_status();deliver();advance(136);
    CHECK(Slave_Water_Phase()==BD_COMPLETE);
    CHECK(Slave_Water_BindPersistedSession(52));
    union_blowdown_init(&master,52,Slave_Water_NowMs());
    union_blowdown_step(&master,1,1,Slave_Water_NowMs());
    read_status();
    CHECK(status[1]==52 && status[2]==0 && (status[0]&15)==UB_IDLE);
    CHECK(master.peer[0].valid && !(union_blowdown_unavailable_mask(&master,Slave_Water_NowMs())&1));
}

int main(void)
{
    const BlowdownConfig config = {10, 100, 30, 200, 20, 100};
    uint16_t completed_request;
    uint8_t completed_phase;
    Slave_Water_Init();
    Slave_Water_SetBlowdownConfig(&config);
    input.pressure_ok = input.heaters_off = 1;
    input.valve_feedback = BD_VALVE_CLOSED;
    CHECK(Slave_Water_BindPersistedSession(42));
    advance(WATER_RECOVERY_MS + 20);
    union_blowdown_init(&master, 42, Slave_Water_NowMs());
    union_blowdown_step(&master, 1, 1, Slave_Water_NowMs());
    deliver(); /* SYNC cannot authorize a valve. */
    CHECK(Slave_Water_Request(0, 1));
    read_status(); CHECK(master.owner == 1 && command[0] == (UB_TAG | UB_SYNC));
    deliver(); CHECK(command[0] == (UB_TAG | UB_GRANT));
    advance(11); CHECK(Slave_Water_ValveOpenAllowed());
    deliver(); /* Exact duplicate does not create another request. */
    advance(125); CHECK(opened && Slave_Water_Phase() == BD_COMPLETE);
    read_status(); CHECK(!master.owner && !master.interlock);
    CHECK(union_blowdown_take_completed(&master, 1, &completed_request, &completed_phase));
    CHECK(completed_request == 1 && completed_phase == UB_COMPLETE);

    /* The next local request may precede receipt of neutral retirement. */
    CHECK(Slave_Water_Request(1, 1));
    deliver(); CHECK(command[0] == UB_TAG);
    CHECK(Slave_Water_Phase() == BD_WAIT_GRANT);
    read_status(); CHECK(master.owner == 1 && master.owner_request == 2);
    deliver(); advance(11); CHECK(Slave_Water_ValveOpenAllowed());

    /* Loss never transfers ownership; cancellation remains tied to request 2. */
    union_blowdown_observe(&master, 1, 0, Slave_Water_NowMs());
    union_blowdown_step(&master, 1, 1, Slave_Water_NowMs());
    CHECK(master.owner == 1 && master.interlock);
    deliver(); CHECK(command[0] == (UB_TAG | UB_CANCEL) && command[2] == 2);
    advance(1); CHECK(!Slave_Water_ValveOpenAllowed());
    read_status(); CHECK(!master.owner && master.interlock);
    CHECK(union_blowdown_take_completed(&master, 1, &completed_request, &completed_phase));
    CHECK(completed_request == 2 && completed_phase == UB_FAULT);
    test_completed_session_rebind();
    puts("PASS master/slave blowdown protocol round trip, requeue and cancellation");
    return 0;
}
