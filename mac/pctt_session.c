/*
 * This file is part of the UWB stack for linux.
 *
 * Copyright (c) 2021-2022 Qorvo US, Inc.
 *
 * This software is provided under the GNU General Public License, version 2
 * (GPLv2), as well as under a Qorvo commercial license.
 *
 * You may choose to use this software under the terms of the GPLv2 License,
 * version 2 ("GPLv2"), as published by the Free Software Foundation.
 * You should have received a copy of the GPLv2 along with this program.  If
 * not, see <http://www.gnu.org/licenses/>.
 *
 * This program is distributed under the GPLv2 in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GPLv2 for more
 * details.
 *
 * If you cannot meet the requirements of the GPLv2, you may not use this
 * software for any purpose without first obtaining a commercial license from
 * Qorvo. Please contact Qorvo to inquire about licensing terms.
 */
#include "pctt_session.h"
#include "pctt_trace.h"
#include "pctt_region.h"
#include "pctt_region_call.h"

#include <linux/errno.h>
#include <linux/ieee802154.h>
#include <linux/string.h>

int pctt_session_init(struct pctt_local *local)
{
	struct pctt_session *session = &local->session;
	struct pctt_session_params *p = &session->params;

	/* Do the same behavior as get_session_state in fira.
	 * INIT state means kzalloc in fira once by session_id.
	 * But as pctt have only one static region. Simulate
	 * kzalloc to do or already done with the local state. */
	if (session->state != PCTT_SESSION_STATE_DEINIT)
		return -EBUSY;

	memset(p, 0, sizeof(*p));
	p->rx_antenna_selection = RX_ANT_SET_ID_DEFAULT;
	p->tx_antenna_selection = TX_ANT_SET_ID_DEFAULT;
	pctt_session_set_state(local, PCTT_SESSION_STATE_INIT);
	return 0;
}

int pctt_session_deinit(struct pctt_local *local)
{
	struct pctt_session *session = &local->session;

	/* Do the same behavior as get_session_state in fira.
	 * DEINIT state means kfree in fira.
	 * But as pctt have only one static region. Simulate
	 * kfree to do or already done with the local state. */
	if (session->state == PCTT_SESSION_STATE_DEINIT)
		return -ENOENT;
	if (session->test_on_going)
		return -EBUSY;

	pctt_session_set_state(local, PCTT_SESSION_STATE_DEINIT);
	return 0;
}

void pctt_session_set_state(struct pctt_local *local,
			    enum pctt_session_state new_state)
{
	struct pctt_session *session = &local->session;
	const enum pctt_session_state old_state = session->state;

	trace_region_pctt_session_set_state(old_state, new_state);

	session->state = new_state;
}

int pctt_session_start_test(struct pctt_local *local, enum pctt_id_attrs cmd_id,
			    const struct genl_info *info)
{
	struct pctt_session *session = &local->session;
	const struct pctt_session_params *p = &session->params;

	trace_region_pctt_session_start_test(cmd_id, p);

	switch (cmd_id) {
	case PCTT_ID_ATTR_PERIODIC_TX:
		break;
	case PCTT_ID_ATTR_LOOPBACK:
		/* Not implemented. */
		return -EINVAL;
	case PCTT_ID_ATTR_SS_TWR:
		if (p->rframe_config != PCTT_RFRAME_CONFIG_SP3)
			return -EINVAL;
		if (!p->slot_duration_dtu)
			return -EINVAL;
		break;
	case PCTT_ID_ATTR_RX:
		break;
	case PCTT_ID_ATTR_PER_RX:
		break;
	default:
		return -EINVAL;
	}

	/* Update unique session context. */
	session->first_access = true;
	/* FIXME: Delete portid_set_once.
	 * See: UWB-2057. */
	session->portid_set_once = true;
	session->event_portid = info->snd_portid;
	/* Set parameters used by the PCTT vendor command. */
	session->setup_hw = (struct dw3000_vendor_cmd_pctt_setup_hw){
		.chan = p->channel_number,
		.rframe_config = p->rframe_config,
		.preamble_code_index = p->preamble_code_index,
		.sfd_id = p->sfd_id,
		.psdu_data_rate = p->psdu_data_rate,
		.preamble_duration = p->preamble_duration,
	};
	/* Update region context. */
	memset(&local->results, 0, sizeof(local->results));
	local->frames_remaining_nb = session->test_params.num_packets;
	session->cmd_id = cmd_id;
	session->test_on_going = true;
	/* At the end, update the state. */
	pctt_session_set_state(local, PCTT_SESSION_STATE_ACTIVE);
	return 0;
}
