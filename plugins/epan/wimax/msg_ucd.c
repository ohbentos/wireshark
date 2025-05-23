/* msg_ucd.c
 * WiMax MAC Management UCD Message decoder
 *
 * Copyright (c) 2007 by Intel Corporation.
 *
 * Author: Lu Pan <lu.pan@intel.com>
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1999 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/* Include files */

#include "config.h"

/*
#define DEBUG
*/

#include <epan/packet.h>
#include <epan/tfs.h>
#include <epan/unit_strings.h>

#include <wsutil/array.h>
#include "wimax_tlv.h"
#include "wimax_mac.h"
#include "wimax_utils.h"
#include "wimax_prefs.h"

void proto_register_mac_mgmt_msg_ucd(void);
void proto_reg_handoff_mac_mgmt_msg_ucd(void);

static dissector_handle_t ucd_handle;

unsigned cqich_id_size;		/* Set for CQICH_Alloc_IE */

static int proto_mac_mgmt_msg_ucd_decoder;
static int ett_mac_mgmt_msg_ucd_decoder;

/* fix fields */
static int hf_ucd_res_timeout;
static int hf_ucd_bw_req_size;
static int hf_ucd_ranging_req_size;
static int hf_ucd_freq;
/* static int hf_ucd_subchan_params_num_chan; */
static int hf_ucd_ul_allocated_subchannles_bitmap;
/* static int hf_ucd_subchan_params_num_sym; */
/* static int hf_ucd_subchan_codes; */

static int hf_ucd_ul_burst_reserved;
static int hf_ucd_ul_burst_uiuc;
static int hf_ucd_burst_fec;
static int hf_ucd_burst_ranging_data_ratio;
/*static int hf_ucd_burst_power_boost;
*static int hf_ucd_burst_tcs_enable;
*/

static int hf_ucd_tlv_t_159_band_amc_allocation_threshold;
static int hf_ucd_tlv_t_158_optional_permutation_ul_allocated_subchannels_bitmap;
static int hf_ucd_tlv_t_160_band_amc_release_threshold;
static int hf_ucd_tlv_t_161_band_amc_allocation_timer;
static int hf_ucd_tlv_t_162_band_amc_release_timer;
static int hf_ucd_tlv_t_163_band_status_report_max_period;
static int hf_ucd_tlv_t_164_band_amc_retry_timer;
static int hf_ucd_tlv_t_171_harq_ack_delay_dl_burst;
static int hf_ucd_tlv_t_170_safety_channel_retry_timer;
static int hf_ucd_tlv_t_172_cqich_band_amc_transition_delay;
static int hf_ucd_tlv_t_174_maximum_retransmission;
static int hf_ucd_tlv_t_177_normalized_cn_override2;
static int hf_ucd_tlv_t_177_normalized_cn_override2_first_line;
static int hf_ucd_tlv_t_177_normalized_cn_override2_list;
static int hf_ucd_tlv_t_176_size_of_cqich_id_field;
static int hf_ucd_tlv_t_186_upper_bound_aas_preamble;
static int hf_ucd_tlv_t_187_lower_bound_aas_preamble;
static int hf_ucd_tlv_t_188_allow_aas_beam_select_message;
static int hf_ucd_tlv_t_189_use_cqich_indication_flag;
static int hf_ucd_tlv_t_190_ms_specific_up_power_addjustment_step;
static int hf_ucd_tlv_t_191_ms_specific_down_power_addjustment_step;
static int hf_ucd_tlv_t_192_min_level_power_offset_adjustment;
static int hf_ucd_tlv_t_193_max_level_power_offset_adjustment;
static int hf_ucd_tlv_t_194_handover_ranging_codes;
static int hf_ucd_tlv_t_195_initial_ranging_interval;
static int hf_ucd_tlv_t_196_tx_power_report;
static int hf_ucd_tlv_t_196_tx_power_report_threshold;
static int hf_ucd_tlv_t_196_tx_power_report_interval;
static int hf_ucd_tlv_t_196_tx_power_report_a_p_avg;
static int hf_ucd_tlv_t_196_tx_power_report_threshold_icqch;
static int hf_ucd_tlv_t_196_tx_power_report_interval_icqch;
static int hf_ucd_tlv_t_196_tx_power_report_a_p_avg_icqch;
/* static int hf_ucd_tlv_t_197_normalized_cn_channel_sounding; */
static int hf_ucd_tlv_t_202_uplink_burst_profile_for_multiple_fec_types;
static int hf_ucd_tlv_t_203_ul_pusc_subchannel_rotation;
static int hf_ucd_tlv_t_205_relative_power_offset_ul_harq_burst;
static int hf_ucd_tlv_t_206_relative_power_offset_ul_burst_containing_mac_mgmt_msg;
static int hf_ucd_tlv_t_207_ul_initial_transmit_timing;
static int hf_ucd_tlv_t_210_fast_feedback_region;
static int hf_ucd_tlv_t_211_harq_ack_region;
static int hf_ucd_tlv_t_212_ranging_region;
static int hf_ucd_tlv_t_213_sounding_region;
static int hf_ucd_tlv_t_150_initial_ranging_codes;
static int hf_ucd_tlv_t_151_periodic_ranging_codes;
static int hf_ucd_tlv_t_152_bandwidth_request_codes;
static int hf_ucd_tlv_t_155_start_of_ranging_codes_group;
static int hf_ucd_tlv_t_156_permutation_base;
static int hf_ucd_ho_ranging_start;
static int hf_ucd_ho_ranging_end;
static int hf_ucd_initial_range_backoff_start;
static int hf_ucd_initial_range_backoff_end;
static int hf_ucd_bandwidth_backoff_start;
static int hf_ucd_bandwidth_backoff_end;
static int hf_ucd_periodic_ranging_backoff_start;
static int hf_ucd_periodic_ranging_backoff_end;
static int hf_ucd_config_change_count;
static int hf_ucd_ranging_backoff_start;
static int hf_ucd_ranging_backoff_end;
static int hf_ucd_request_backoff_start;
static int hf_ucd_request_backoff_end;

/* static int hf_ucd_unknown_type; */
static int hf_ucd_invalid_tlv;

#if 0
static const value_string vals_dcd_burst_tcs[] =
{
	{0, "TCS disabled"},
	{1, "TCS enabled"},
	{0,  NULL}
};
#endif

static const value_string vals_dcd_burst_fec[] =
{
	{ 0, "QPSK (CC) 1/2"},
	{ 1, "QPSK (CC) 3/4"},
	{ 2, "16-QAM (CC) 1/2"},
	{ 3, "16-QAM (CC) 3/4"},
	{ 4, "64-QAM (CC) 1/2"},
	{ 5, "64-QAM (CC) 2/3"},
	{ 6, "64-QAM (CC) 3/4"},
	{ 7, "QPSK (BTC) 1/2"},
	{ 8, "QPSK (BTC) 3/4 or 2/3"},
	{ 9, "16-QAM (BTC) 3/5"},
	{10, "16-QAM (BTC) 4/5"},
	{11, "64-QAM (BTC) 2/3 or 5/8"},
	{12, "64-QAM (BTC) 5/6 or 4/5"},
	{13, "QPSK (CTC) 1/2"},
	{14, "Reserved"},
	{15, "QPSK (CTC) 3/4"},
	{16, "16-QAM (CTC) 1/2"},
	{17, "16-QAM (CTC) 3/4"},
	{18, "64-QAM (CTC) 1/2"},
	{19, "64-QAM (CTC) 2/3"},
	{20, "64-QAM (CTC) 3/4"},
	{21, "64-QAM (CTC) 5/6"},
	{22, "QPSK (ZT CC) 1/2"},
	{23, "QPSK (ZT CC) 3/4"},
	{24, "16-QAM (ZT CC) 1/2"},
	{25, "16-QAM (ZT CC) 3/4"},
	{26, "64-QAM (ZT CC) 1/2"},
	{27, "64-QAM (ZT CC) 2/3"},
	{28, "64-QAM (ZT CC) 3/4"},
	{29, "QPSK (LDPC) 1/2"},
	{30, "QPSK (LDPC) 2/3 A code"},
	{31, "16-QAM (LDPC) 3/4 A code"},
	{32, "16-QAM (LDPC) 1/2"},
	{33, "16-QAM (LDPC) 2/3 A code"},
	{34, "16-QAM (LDPC) 3/4 A code"},
	{35, "64-QAM (LDPC) 1/2"},
	{36, "64-QAM (LDPC) 2/3 A code"},
	{37, "64-QAM (LDPC) 3/4 A code"},
	{38, "QPSK (LDPC) 2/3 B code"},
	{39, "QPSK (LDPC) 3/4 B code"},
	{40, "16-QAM (LDPC) 2/3 B code"},
	{41, "16-QAM (LDPC) 3/4 B code"},
	{42, "64-QAM (LDPC) 2/3 B code"},
	{43, "64-QAM (LDPC) 3/4 B code"},
	{44, "QPSK (CC with optional interleaver) 1/2"},
	{45, "QPSK (CC with optional interleaver) 3/4"},
	{46, "16-QAM (CC with optional interleaver) 1/2"},
	{47, "16-QAM (CC optional interleaver) 0%00"},
	{48, "64-QAM (CC with optional interleaver) 2/3"},
	{49, "64-QAM (CC with optional interleaver) 3/4"},
	{50, "QPSK (LDPC) 5/6"},
	{51, "16-QAM (LDPC) 5/6"},
	{52, "64-QAM (LDPC) 5/6"},
	{0,  NULL}
};

static const value_string vals_ucd_cqich_size[] =
{
	{0, "0 bits"},
	{1, "3 bits"},
	{2, "4 bits"},
	{3, "5 bits"},
	{4, "6 bits"},
	{5, "7 bits"},
	{6, "8 bits"},
	{7, "9 bits"},
	{0,  NULL}
};

static const value_string vals_yes_no_str[] =
{
	{0, "No"},
	{1, "Yes"},
	{0,  NULL}
};


/* UCD dissector */
static int dissect_mac_mgmt_msg_ucd_decoder(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, void* data _U_)
{
	unsigned offset = 0;
	unsigned tvb_len, length;
	int   tlv_type, tlv_len, tlv_offset, tlv_value_offset;
	tlv_info_t tlv_info;
	char* proto_str;

	{	/* we are being asked for details */
		proto_item *ucd_item;
		proto_tree *ucd_tree;
		unsigned ucd_ranging_backoff_start;
		unsigned ucd_ranging_backoff_end;
		unsigned ucd_request_backoff_start;
		unsigned ucd_request_backoff_end;

		tvb_len =  tvb_reported_length(tvb);
		/* display MAC payload type UCD */
		ucd_item = proto_tree_add_protocol_format(tree, proto_mac_mgmt_msg_ucd_decoder, tvb, offset, -1, "Uplink Channel Descriptor (UCD)");
		ucd_tree = proto_item_add_subtree(ucd_item, ett_mac_mgmt_msg_ucd_decoder);

		/* Decode and display the Uplink Channel Descriptor (UCD) */
		/* display the Configuration Change Count */
		proto_tree_add_item(ucd_tree, hf_ucd_config_change_count, tvb, offset, 1, ENC_NA);
		offset++;

		/* get the ranging backoff start */
		ucd_ranging_backoff_start = tvb_get_uint8(tvb, offset);
		proto_tree_add_uint_format_value(ucd_tree, hf_ucd_ranging_backoff_start, tvb, offset, 1, (1 << ucd_ranging_backoff_start), "2^%u = %u", ucd_ranging_backoff_start, (1 << ucd_ranging_backoff_start));
		offset++;

		/* get the ranging backoff end */
		ucd_ranging_backoff_end = tvb_get_uint8(tvb, offset);
		proto_tree_add_uint_format_value(ucd_tree, hf_ucd_ranging_backoff_end, tvb, offset, 1, (1 << ucd_ranging_backoff_end), "2^%u = %u", ucd_ranging_backoff_end, (1 << ucd_ranging_backoff_end));
		offset++;

		/* get the request backoff start */
		ucd_request_backoff_start = tvb_get_uint8(tvb, offset);
		proto_tree_add_uint_format_value(ucd_tree, hf_ucd_request_backoff_start, tvb, offset, 1, (1 << ucd_request_backoff_start), "2^%u = %u", ucd_request_backoff_start, (1 << ucd_request_backoff_start));
		offset++;

		/* get the request backoff end */
		ucd_request_backoff_end = tvb_get_uint8(tvb, offset);
		proto_tree_add_uint_format_value(ucd_tree, hf_ucd_request_backoff_end, tvb, offset, 1, (1 << ucd_request_backoff_end), "2^%u = %u", ucd_request_backoff_end, (1 << ucd_request_backoff_end));
		offset++;

		while(offset < tvb_len)
		{
			proto_tree *tlv_tree;
			proto_item *tlv_item1;
			unsigned ul_burst_uiuc;
			unsigned utemp;
			/* get the TLV information */
			init_tlv_info(&tlv_info, tvb, offset);
			/* get the TLV type */
			tlv_type = get_tlv_type(&tlv_info);
			/* get the TLV length */
			tlv_len = get_tlv_length(&tlv_info);
			if(tlv_type == -1 || tlv_len > MAX_TLV_LEN || tlv_len < 1)
			{	/* invalid tlv info */
				col_append_sep_str(pinfo->cinfo, COL_INFO, NULL, "UCD TLV error");
				proto_tree_add_item(ucd_tree,hf_ucd_invalid_tlv, tvb, offset, (tvb_len - offset), ENC_NA);
				break;
			}
			/* get the TLV value offset */
			tlv_value_offset = get_tlv_value_offset(&tlv_info);
#ifdef DEBUG /* for debug only */
			proto_tree_add_protocol_format(ucd_tree, proto_mac_mgmt_msg_ucd_decoder, tvb, offset, (tlv_len + tlv_value_offset), "UCD Type: %u (%u bytes, offset=%u, tvb_len=%u)", tlv_type, tlv_len, offset, tvb_len);
#endif
			/* update the offset */
			offset += tlv_value_offset;
			/* process UCD TLV Encoded information */
			if (include_cor2_changes)
			{
				switch (tlv_type)
				{
					case UCD_TLV_T_203_UL_PUSC_SUBCHANNEL_ROTATION:
					{
						add_tlv_subtree(&tlv_info, ucd_tree, hf_ucd_tlv_t_203_ul_pusc_subchannel_rotation, tvb, offset-tlv_value_offset, ENC_BIG_ENDIAN);
						break;
					}
					case UCD_TLV_T_205_RELATIVE_POWER_OFFSET_UL_HARQ_BURST:
					{
						add_tlv_subtree(&tlv_info, ucd_tree, hf_ucd_tlv_t_205_relative_power_offset_ul_harq_burst, tvb, offset-tlv_value_offset, ENC_BIG_ENDIAN);
						break;
					}
					case UCD_TLV_T_206_RELATIVE_POWER_OFFSET_UL_BURST_CONTAINING_MAC_MGMT_MSG:
					{
						add_tlv_subtree(&tlv_info, ucd_tree, hf_ucd_tlv_t_206_relative_power_offset_ul_burst_containing_mac_mgmt_msg, tvb, offset-tlv_value_offset, ENC_BIG_ENDIAN);
						break;
					}
					case UCD_TLV_T_207_UL_INITIAL_TRANSMIT_TIMING:
					{
						add_tlv_subtree(&tlv_info, ucd_tree, hf_ucd_tlv_t_207_ul_initial_transmit_timing, tvb, offset-tlv_value_offset, ENC_BIG_ENDIAN);
						break;
					}
					case UCD_TLV_T_210_FAST_FEEDBACK_REGION:
					{
						add_tlv_subtree(&tlv_info, ucd_tree, hf_ucd_tlv_t_210_fast_feedback_region, tvb, offset-tlv_value_offset, ENC_NA);
						break;
					}
					case UCD_TLV_T_211_HARQ_ACK_REGION:
					{
						add_tlv_subtree(&tlv_info, ucd_tree, hf_ucd_tlv_t_211_harq_ack_region, tvb, offset-tlv_value_offset, ENC_NA);
						break;
					}
					case UCD_TLV_T_212_RANGING_REGION:
					{
						add_tlv_subtree(&tlv_info, ucd_tree, hf_ucd_tlv_t_212_ranging_region, tvb, offset-tlv_value_offset, ENC_NA);
						break;
					}
					case UCD_TLV_T_213_SOUNDING_REGION:
					{
						add_tlv_subtree(&tlv_info, ucd_tree, hf_ucd_tlv_t_213_sounding_region, tvb, offset-tlv_value_offset, ENC_NA);
						break;
					}
				}
			}
			switch (tlv_type)
			{
				case UCD_UPLINK_BURST_PROFILE:
				{
					/* get the UIUC */
					ul_burst_uiuc = tvb_get_uint8(tvb, offset) & 0x0F;
					/* add TLV subtree */
					proto_str = wmem_strdup_printf(pinfo->pool, "Uplink Burst Profile (UIUC = %u)", ul_burst_uiuc);
					tlv_tree = add_protocol_subtree(&tlv_info, ett_mac_mgmt_msg_ucd_decoder, ucd_tree, proto_mac_mgmt_msg_ucd_decoder, tvb, offset-tlv_value_offset, tlv_len, proto_str);
					proto_tree_add_item(tlv_tree, hf_ucd_ul_burst_reserved, tvb, offset, 1, ENC_BIG_ENDIAN);
					proto_tree_add_item(tlv_tree, hf_ucd_ul_burst_uiuc, tvb, offset, 1, ENC_BIG_ENDIAN);
					for (tlv_offset = 1; tlv_offset < tlv_len;)
					{	/* get the TLV information */
						init_tlv_info(&tlv_info, tvb, (offset+tlv_offset));
						/* get the TLV type */
						tlv_type = get_tlv_type(&tlv_info);
						if(tlv_type == -1)
						{	/* invalid tlv info */
							col_append_sep_str(pinfo->cinfo, COL_INFO, NULL, "UL Burst Profile error");
							proto_tree_add_item(tlv_tree, hf_ucd_invalid_tlv, tvb, offset, (tlv_len - offset - tlv_offset), ENC_NA);
							break;
						}
						/* get the TLV length */
						length = get_tlv_length(&tlv_info);

						switch (tlv_type)
						{
							case UCD_BURST_FEC:
							{
								add_tlv_subtree(&tlv_info, tlv_tree, hf_ucd_burst_fec, tvb, (offset+tlv_offset), ENC_BIG_ENDIAN);
								break;
							}
							case UCD_BURST_RANGING_DATA_RATIO:
							{
								add_tlv_subtree(&tlv_info, tlv_tree, hf_ucd_burst_ranging_data_ratio, tvb, (offset+tlv_offset), ENC_BIG_ENDIAN);
								break;
							}
#if 0 /* for OFDM */
							case UCD_BURST_POWER_BOOST:
							{
								add_tlv_subtree(&tlv_info, tlv_tree, hf_ucd_burst_power_boost, tvb, (offset+tlv_offset), ENC_BIG_ENDIAN);
								break;
							}
							case UCD_BURST_TCS_ENABLE:
							{
								add_tlv_subtree(&tlv_info, tlv_tree, hf_ucd_burst_tcs_enable, tvb, (offset+tlv_offset), 1, ENC_BIG_ENDIAN);
								break;
							}
#endif
							default:
								/* ??? */
								break;
						}
						tlv_offset += (length+get_tlv_value_offset(&tlv_info));
					}
					break;
				}
				case UCD_RESERVATION_TIMEOUT:
				{
					add_tlv_subtree(&tlv_info, ucd_tree, hf_ucd_res_timeout, tvb, offset-tlv_value_offset, ENC_BIG_ENDIAN);
					break;
				}
				case UCD_BW_REQ_SIZE:
				{
					add_tlv_subtree(&tlv_info, ucd_tree, hf_ucd_bw_req_size, tvb, offset-tlv_value_offset, ENC_BIG_ENDIAN);
					break;
				}
				case UCD_RANGING_REQ_SIZE:
				{
					add_tlv_subtree(&tlv_info, ucd_tree, hf_ucd_ranging_req_size, tvb, offset-tlv_value_offset, ENC_BIG_ENDIAN);
					break;
				}
				case UCD_FREQUENCY:
				{
					add_tlv_subtree(&tlv_info, ucd_tree, hf_ucd_freq, tvb, offset-tlv_value_offset, ENC_BIG_ENDIAN);
					break;
				}
				case UCD_TLV_T_7_HO_RANGING_START:
				{
					tlv_tree = add_tlv_subtree_no_item(&tlv_info, ucd_tree, hf_ucd_ho_ranging_start, tvb, offset-tlv_value_offset);
					utemp = tvb_get_uint8(tvb, offset);
					proto_tree_add_uint_format_value(tlv_tree, hf_ucd_ho_ranging_start, tvb, offset, tvb_len, utemp, "2^%u = %u", utemp, (1 << utemp));
					break;
				}
				case UCD_TLV_T_8_RANGING_HO_END:
				{
					tlv_tree = add_tlv_subtree_no_item(&tlv_info, ucd_tree, hf_ucd_ho_ranging_end, tvb, offset-tlv_value_offset);
					utemp = tvb_get_uint8(tvb, offset);
					proto_tree_add_uint_format_value(tlv_tree, hf_ucd_ho_ranging_end, tvb, offset, tvb_len, utemp, "2^%u = %u", utemp, (1 << utemp));
					break;
				}
				case UCD_TLV_T_158_OPTIONAL_PERMUTATION_UL_ALLOCATED_SUBCHANNELS_BITMAP:
				{
					add_tlv_subtree(&tlv_info, ucd_tree, hf_ucd_tlv_t_158_optional_permutation_ul_allocated_subchannels_bitmap, tvb, offset-tlv_value_offset, ENC_NA);
					break;
				}
				case UCD_TLV_T_159_BAND_AMC_ALLOCATION_THRESHOLD:
				{
					add_tlv_subtree(&tlv_info, ucd_tree, hf_ucd_tlv_t_159_band_amc_allocation_threshold, tvb, offset-tlv_value_offset, ENC_BIG_ENDIAN);
					break;
				}
				case UCD_TLV_T_160_BAND_AMC_RELEASE_THRESHOLD:
				{
					add_tlv_subtree(&tlv_info, ucd_tree, hf_ucd_tlv_t_160_band_amc_release_threshold, tvb, offset-tlv_value_offset, ENC_BIG_ENDIAN);
					break;
				}
				case UCD_TLV_T_161_BAND_AMC_ALLOCATION_TIMER:
				{
					add_tlv_subtree(&tlv_info, ucd_tree, hf_ucd_tlv_t_161_band_amc_allocation_timer, tvb, offset-tlv_value_offset, ENC_BIG_ENDIAN);
					break;
				}
				case UCD_TLV_T_162_BAND_AMC_RELEASE_TIMER:
				{
					add_tlv_subtree(&tlv_info, ucd_tree, hf_ucd_tlv_t_162_band_amc_release_timer, tvb, offset-tlv_value_offset, ENC_BIG_ENDIAN);
					break;
				}
				case UCD_TLV_T_163_BAND_STATUS_REPORT_MAX_PERIOD:
				{
					add_tlv_subtree(&tlv_info, ucd_tree, hf_ucd_tlv_t_163_band_status_report_max_period, tvb, offset-tlv_value_offset, ENC_BIG_ENDIAN);
					break;
				}
				case UCD_TLV_T_164_BAND_AMC_RETRY_TIMER:
				{
					add_tlv_subtree(&tlv_info, ucd_tree, hf_ucd_tlv_t_164_band_amc_retry_timer, tvb, offset-tlv_value_offset, ENC_BIG_ENDIAN);
					break;
				}
				case UCD_TLV_T_170_SAFETY_CHANNEL_RETRY_TIMER:
				{
					add_tlv_subtree(&tlv_info, ucd_tree, hf_ucd_tlv_t_170_safety_channel_retry_timer, tvb, offset-tlv_value_offset, ENC_BIG_ENDIAN);
					break;
				}
				case UCD_TLV_T_171_HARQ_ACK_DELAY_FOR_DL_BURST:
				{
					add_tlv_subtree(&tlv_info, ucd_tree, hf_ucd_tlv_t_171_harq_ack_delay_dl_burst, tvb, offset-tlv_value_offset, ENC_BIG_ENDIAN);
					break;
				}
				case UCD_TLV_T_172_CQICH_BAND_AMC_TRANSITION_DELAY:
				{
					add_tlv_subtree(&tlv_info, ucd_tree, hf_ucd_tlv_t_172_cqich_band_amc_transition_delay, tvb, offset-tlv_value_offset, ENC_BIG_ENDIAN);
					break;
				}
				case UCD_TLV_T_174_MAXIMUM_RETRANSMISSION:
				{
					add_tlv_subtree(&tlv_info, ucd_tree, hf_ucd_tlv_t_174_maximum_retransmission, tvb, offset-tlv_value_offset, ENC_BIG_ENDIAN);
					break;
				}
				case UCD_TLV_T_176_SIZE_OF_CQICH_ID_FIELD:
				{
					utemp = tvb_get_uint8(tvb, offset);
					cqich_id_size = 0;	/* Default is 0 */
					if (utemp && utemp < 8) {
					    /* Set for CQICH_Alloc_IE */
					    cqich_id_size = utemp + 2;
					}
					add_tlv_subtree(&tlv_info, ucd_tree, hf_ucd_tlv_t_176_size_of_cqich_id_field, tvb, offset-tlv_value_offset, ENC_BIG_ENDIAN);
					break;
				}
				case UCD_TLV_T_177_NORMALIZED_CN_OVERRIDE_2:
				{
					/* add TLV subtree */
					tlv_item1 = add_tlv_subtree(&tlv_info, ucd_tree, hf_ucd_tlv_t_177_normalized_cn_override2, tvb, offset-tlv_value_offset, ENC_NA|ENC_ASCII);
					tlv_tree = proto_item_add_subtree(tlv_item1, ett_mac_mgmt_msg_ucd_decoder);
					proto_tree_add_item(tlv_tree, hf_ucd_tlv_t_177_normalized_cn_override2_first_line, tvb, offset + 2, 1, ENC_BIG_ENDIAN);
					proto_tree_add_item(tlv_tree, hf_ucd_tlv_t_177_normalized_cn_override2_list, tvb, offset + 3, 7, ENC_ASCII|ENC_NA);
					break;
				}
				case UCD_TLV_T_186_UPPER_BOUND__AAS_PREAMBLE:
				{
					add_tlv_subtree(&tlv_info, ucd_tree, hf_ucd_tlv_t_186_upper_bound_aas_preamble, tvb, offset-tlv_value_offset, ENC_BIG_ENDIAN);
					break;
				}
				case UCD_TLV_T_187_LOWER_BOUND_AAS_PREAMBLE:
				{
					add_tlv_subtree(&tlv_info, ucd_tree, hf_ucd_tlv_t_187_lower_bound_aas_preamble, tvb, offset-tlv_value_offset, ENC_BIG_ENDIAN);
					break;
				}
				case UCD_TLV_T_188_ALLOW_AAS_BEAM_SELECT_MESSAGE:
				{
					add_tlv_subtree(&tlv_info, ucd_tree, hf_ucd_tlv_t_188_allow_aas_beam_select_message, tvb, offset-tlv_value_offset, ENC_BIG_ENDIAN);
					break;
				}
				case UCD_TLV_T_189_USE_CQICH_INDICATION_FLAG:
				{
					add_tlv_subtree(&tlv_info, ucd_tree, hf_ucd_tlv_t_189_use_cqich_indication_flag, tvb, offset-tlv_value_offset, ENC_BIG_ENDIAN);
					break;
				}
				case UCD_TLV_T_190_MS_SPECIFIC_UP_POWER_OFFSET_ADJUSTMENT_STEP:
				{
					add_tlv_subtree(&tlv_info, ucd_tree, hf_ucd_tlv_t_190_ms_specific_up_power_addjustment_step, tvb, offset-tlv_value_offset, ENC_BIG_ENDIAN);
					break;
				}
				case UCD_TLV_T_191_MS_SPECIFIC_DOWN_POWER_OFSET_ADJUSTMENT_STEP:
				{
					add_tlv_subtree(&tlv_info, ucd_tree, hf_ucd_tlv_t_191_ms_specific_down_power_addjustment_step, tvb, offset-tlv_value_offset, ENC_BIG_ENDIAN);
					break;
				}
				case UCD_TLV_T_192_MIN_LEVEL_POWER_OFFSET_ADJUSTMENT:
				{
					add_tlv_subtree(&tlv_info, ucd_tree, hf_ucd_tlv_t_192_min_level_power_offset_adjustment, tvb, offset-tlv_value_offset, ENC_BIG_ENDIAN);
					break;
				}
				case UCD_TLV_T_193_MAX_LEVEL_POWER_OFFSETR_ADJUSTMENT:
				{
					add_tlv_subtree(&tlv_info, ucd_tree, hf_ucd_tlv_t_193_max_level_power_offset_adjustment, tvb, offset-tlv_value_offset, ENC_BIG_ENDIAN);
					break;
				}
				case UCD_TLV_T_194_HANDOVER_RANGING_CODES:
				{
					add_tlv_subtree(&tlv_info, ucd_tree, hf_ucd_tlv_t_194_handover_ranging_codes, tvb, offset-tlv_value_offset, ENC_BIG_ENDIAN);
					break;
				}
				case UCD_TLV_T_195_INITIAL_RANGING_INTERVAL:
				{
					add_tlv_subtree(&tlv_info, ucd_tree, hf_ucd_tlv_t_195_initial_ranging_interval, tvb, offset-tlv_value_offset, ENC_BIG_ENDIAN);
					break;
				}
				case UCD_TLV_T_196_TX_POWER_REPORT:
				{
					tlv_item1 = add_tlv_subtree(&tlv_info, ucd_tree, hf_ucd_tlv_t_196_tx_power_report, tvb, offset-tlv_value_offset, ENC_BIG_ENDIAN);
					tlv_tree = proto_item_add_subtree(tlv_item1, ett_mac_mgmt_msg_ucd_decoder);
					proto_tree_add_item(tlv_tree, hf_ucd_tlv_t_196_tx_power_report_threshold, tvb, offset, 1, ENC_BIG_ENDIAN);
					proto_tree_add_item(tlv_tree, hf_ucd_tlv_t_196_tx_power_report_interval, tvb , offset, 1, ENC_BIG_ENDIAN);
					proto_tree_add_item(tlv_tree, hf_ucd_tlv_t_196_tx_power_report_a_p_avg, tvb, (offset + 1), 1, ENC_BIG_ENDIAN);
					proto_tree_add_item(tlv_tree, hf_ucd_tlv_t_196_tx_power_report_threshold_icqch, tvb, (offset + 1), 1, ENC_BIG_ENDIAN);
					proto_tree_add_item(tlv_tree, hf_ucd_tlv_t_196_tx_power_report_interval_icqch, tvb, (offset + 2), 1, ENC_BIG_ENDIAN);
					proto_tree_add_item(tlv_tree, hf_ucd_tlv_t_196_tx_power_report_a_p_avg_icqch, tvb, (offset + 2), 1, ENC_BIG_ENDIAN);
					break;
				}
				case UCD_TLV_T_197_NORMALIZED_CN_FOR_CHANNEL_SOUNDING:
				{
					add_tlv_subtree(&tlv_info, ucd_tree, hf_ucd_tlv_t_195_initial_ranging_interval, tvb, offset-tlv_value_offset, ENC_BIG_ENDIAN);
					break;
				}
				case UCD_TLV_T_198_INTIAL_RANGING_BACKOFF_START:
				{
					tlv_tree = add_tlv_subtree_no_item(&tlv_info, ucd_tree, hf_ucd_initial_range_backoff_start, tvb, offset-tlv_value_offset);
					utemp = tvb_get_uint8(tvb, offset);
					proto_tree_add_uint_format_value(tlv_tree, hf_ucd_initial_range_backoff_start, tvb, offset, tvb_len, utemp, "2^%u = %u", utemp, (1 << utemp));
					break;
				}
				case UCD_TLV_T_199_INITIAL_RANGING_BACKOFF_END:
				{
					tlv_tree = add_tlv_subtree_no_item(&tlv_info, ucd_tree, hf_ucd_initial_range_backoff_end, tvb, offset-tlv_value_offset);
					utemp = tvb_get_uint8(tvb, offset);
					proto_tree_add_uint_format_value(tlv_tree, hf_ucd_initial_range_backoff_end, tvb, offset, tvb_len, utemp, "2^%u = %u", utemp, (1 << utemp));
					break;
				}
				case UCD_TLV_T_200_BANDWIDTH_REQUESET_BACKOFF_START:
				{
					tlv_tree = add_tlv_subtree_no_item(&tlv_info, ucd_tree, hf_ucd_bandwidth_backoff_start, tvb, offset-tlv_value_offset);
					utemp = tvb_get_uint8(tvb, offset);
					proto_tree_add_uint_format_value(tlv_tree, hf_ucd_bandwidth_backoff_start, tvb, offset, tvb_len, utemp, "2^%u = %u", utemp, (1 << utemp));
					break;
				}
				case UCD_TLV_T_201_BANDWIDTH_REQUEST_BACKOFF_END:
				{
					tlv_tree = add_tlv_subtree_no_item(&tlv_info, ucd_tree, hf_ucd_bandwidth_backoff_end, tvb, offset-tlv_value_offset);
					utemp = tvb_get_uint8(tvb, offset);
					proto_tree_add_uint_format_value(tlv_tree, hf_ucd_bandwidth_backoff_end, tvb, offset, tvb_len, utemp, "2^%u = %u", utemp, (1 << utemp));
					break;
				}
				case UCD_TLV_T_202_UPLINK_BURST_PROFILE_FOR_MULTIPLE_FEC_TYPES:
				{
					add_tlv_subtree(&tlv_info, ucd_tree, hf_ucd_tlv_t_202_uplink_burst_profile_for_multiple_fec_types, tvb, offset-tlv_value_offset, ENC_BIG_ENDIAN);
					break;
				}
				case UCD_INITIAL_RANGING_CODES:
				{
					add_tlv_subtree(&tlv_info, ucd_tree, hf_ucd_tlv_t_150_initial_ranging_codes, tvb, offset-tlv_value_offset, ENC_BIG_ENDIAN);
					break;
				}
				case UCD_PERIODIC_RANGING_CODES:
				{
					add_tlv_subtree(&tlv_info, ucd_tree, hf_ucd_tlv_t_151_periodic_ranging_codes, tvb, offset-tlv_value_offset, ENC_BIG_ENDIAN);
					break;
				}
				case UCD_BANDWIDTH_REQUEST_CODES:
				{
					add_tlv_subtree(&tlv_info, ucd_tree, hf_ucd_tlv_t_152_bandwidth_request_codes, tvb, offset-tlv_value_offset, ENC_BIG_ENDIAN);
					break;
				}
				case UCD_PERIODIC_RANGING_BACKOFF_START:
				{
					tlv_tree = add_tlv_subtree_no_item(&tlv_info, ucd_tree, hf_ucd_periodic_ranging_backoff_start, tvb, offset-tlv_value_offset);
					utemp = tvb_get_uint8(tvb, offset);
					proto_tree_add_uint_format_value(tlv_tree, hf_ucd_periodic_ranging_backoff_start, tvb, offset, tvb_len, utemp, "2^%u = %u", utemp, (1 << utemp));
					break;

				}
				case UCD_PERIODIC_RANGING_BACKOFF_END:
				{
					tlv_tree = add_tlv_subtree_no_item(&tlv_info, ucd_tree, hf_ucd_periodic_ranging_backoff_end, tvb, offset-tlv_value_offset);
					utemp = tvb_get_uint8(tvb, offset);
					proto_tree_add_uint_format_value(tlv_tree, hf_ucd_periodic_ranging_backoff_end, tvb, offset, tvb_len, utemp, "2^%u = %u", utemp, (1 << utemp));
					break;
				}
				case UCD_START_OF_RANGING_CODES_GROUP:
				{
					add_tlv_subtree(&tlv_info, ucd_tree, hf_ucd_tlv_t_155_start_of_ranging_codes_group, tvb, offset-tlv_value_offset, ENC_BIG_ENDIAN);
					break;

				}
				case UCD_PERMUTATION_BASE:
				{
					add_tlv_subtree(&tlv_info, ucd_tree, hf_ucd_tlv_t_156_permutation_base, tvb, offset-tlv_value_offset, ENC_BIG_ENDIAN);
					break;
				}
				case UCD_UL_ALLOCATED_SUBCHANNELS_BITMAP:
				{
					add_tlv_subtree(&tlv_info, ucd_tree, hf_ucd_ul_allocated_subchannles_bitmap, tvb, offset-tlv_value_offset, ENC_NA);
					break;
				}
				case UCD_TLV_T_203_UL_PUSC_SUBCHANNEL_ROTATION:
				case UCD_TLV_T_205_RELATIVE_POWER_OFFSET_UL_HARQ_BURST:
				case UCD_TLV_T_206_RELATIVE_POWER_OFFSET_UL_BURST_CONTAINING_MAC_MGMT_MSG:
				case UCD_TLV_T_207_UL_INITIAL_TRANSMIT_TIMING:
				case UCD_TLV_T_210_FAST_FEEDBACK_REGION:
				case UCD_TLV_T_211_HARQ_ACK_REGION:
				case UCD_TLV_T_212_RANGING_REGION:
				case UCD_TLV_T_213_SOUNDING_REGION:
				{
					/* Unknown TLV type if cor2 not enabled. */
					if (!include_cor2_changes)
					{
						add_tlv_subtree(&tlv_info, ucd_tree, hf_ucd_invalid_tlv, tvb, offset-tlv_value_offset, ENC_NA);
					}
					break;
				}
				default:
				{
					add_tlv_subtree(&tlv_info, ucd_tree, hf_ucd_invalid_tlv, tvb, offset-tlv_value_offset, ENC_NA);
				}
			}	/* end of switch(tlv_type) */
			offset += tlv_len;
		}	/* end of TLV process while loop */
	}
	return tvb_captured_length(tvb);
}

/* Register Wimax Mac Payload Protocol and Dissector */
void proto_register_mac_mgmt_msg_ucd(void)
{
	/* UCD display */
	static hf_register_info hf[] =
	{
		{
			&hf_ucd_tlv_t_188_allow_aas_beam_select_message,
			{
				"Allow AAS Beam Select Message", "wmx.ucd.allow_aas_beam_select_message",
				FT_INT8, BASE_DEC, VALS(vals_yes_no_str), 0, NULL, HFILL
			}
		},
		{
			&hf_ucd_tlv_t_159_band_amc_allocation_threshold,
			{
				"Band AMC Allocation Threshold", "wmx.ucd.band_amc.allocation_threshold",
				FT_UINT8, BASE_HEX|BASE_UNIT_STRING, UNS(&wimax_units_db), 0, NULL, HFILL
			}
		},
		{
			&hf_ucd_tlv_t_161_band_amc_allocation_timer,
			{
				"Band AMC Allocation Timer", "wmx.ucd.band_amc.allocation_timer",
				FT_UINT8, BASE_HEX|BASE_UNIT_STRING, UNS(&wimax_units_frame_frames), 0, NULL, HFILL
			}
		},
		{
			&hf_ucd_tlv_t_160_band_amc_release_threshold,
			{
				"Band AMC Release Threshold", "wmx.ucd.band_amc.release_threshold",
				FT_UINT8, BASE_HEX|BASE_UNIT_STRING, UNS(&wimax_units_db), 0, NULL, HFILL
			}
		},
		{
			&hf_ucd_tlv_t_162_band_amc_release_timer,
			{
				"Band AMC Release Timer", "wmx.ucd.band_amc.release_timer",
				FT_UINT8, BASE_HEX|BASE_UNIT_STRING, UNS(&wimax_units_frame_frames), 0, NULL, HFILL
			}
		},
		{
			&hf_ucd_tlv_t_164_band_amc_retry_timer,
			{
				"Band AMC Retry Timer", "wmx.ucd.band_amc.retry_timer",
				FT_UINT8, BASE_HEX|BASE_UNIT_STRING, UNS(&wimax_units_frame_frames), 0, NULL, HFILL
			}
		},
		{
			&hf_ucd_tlv_t_163_band_status_report_max_period,
			{
				"Band Status Report MAC Period", "wmx.ucd.band_status.report_max_period",
				FT_UINT8, BASE_DEC|BASE_UNIT_STRING, UNS(&wimax_units_frame_frames), 0, NULL, HFILL
			}
		},
		{
			&hf_ucd_tlv_t_152_bandwidth_request_codes,
			{
				"Bandwidth Request Codes", "wmx.ucd.bandwidth_request",
				FT_UINT8, BASE_HEX, NULL, 0, NULL, HFILL
			}
		},
		{
			&hf_ucd_burst_fec,
			{
				"FEC Code Type", "wmx.ucd.burst.fec",
				FT_UINT8, BASE_HEX, VALS(vals_dcd_burst_fec), 0, NULL, HFILL
			}
		},
		{
			&hf_ucd_burst_ranging_data_ratio,
			{
				"Ranging Data Ratio", "wmx.ucd.burst.ranging_data_ratio",
				FT_UINT8, BASE_DEC|BASE_UNIT_STRING, UNS(&wimax_units_db), 0, NULL, HFILL
			}
		},
		{
			&hf_ucd_ul_burst_reserved,
			{
				"Reserved", "wmx.ucd.burst.reserved",
				FT_UINT8, BASE_HEX, NULL, 0xF0, NULL, HFILL
			}
		},
		{
			&hf_ucd_ul_burst_uiuc,
			{
				"UIUC", "wmx.ucd.burst.uiuc",
				FT_UINT8, BASE_DEC, NULL, 0x0F, NULL, HFILL
			}
		},
#if 0
		{
			&hf_ucd_burst_power_boost,
			{"Focused Contention Power Boost", "wmx.ucd.burst.power_boost", FT_UINT8, BASE_HEX|BASE_UNIT_STRING, UNS(&wimax_units_db), 0, NULL, HFILL}
		},
		{
			&hf_ucd_burst_tcs_enable,
			{"TCS", "wmx.ucd.burst.tcs", FT_UINT8, BASE_DEC, VALS(vals_dcd_burst_tcs), 0, NULL, HFILL}
		},
#endif
		{
			&hf_ucd_bw_req_size,
			{
				"Bandwidth Request Opportunity Size", "wmx.ucd.bw_req_size",
				FT_UINT16, BASE_DEC|BASE_UNIT_STRING, UNS(&wimax_units_ps), 0, NULL, HFILL
			}
		},
		{
			&hf_ucd_tlv_t_172_cqich_band_amc_transition_delay,
			{
				"CQICH Band AMC-Transition Delay", "wmx.ucd.cqich_band_amc_transition_delay",
				FT_UINT8, BASE_DEC|BASE_UNIT_STRING, UNS(&wimax_units_frame_frames), 0, NULL, HFILL
			}
		},
		{
			&hf_ucd_freq,
			{
				"Frequency", "wmx.ucd.frequency",
				FT_UINT32, BASE_DEC|BASE_UNIT_STRING, UNS(&wimax_units_khz), 0, NULL, HFILL
			}
		},
		{
			&hf_ucd_tlv_t_194_handover_ranging_codes,
			{
				"Handover Ranging Codes", "wmx.ucd.handover_ranging_codes",
				FT_INT8, BASE_DEC, NULL, 0, NULL, HFILL
			}
		},
		{
			&hf_ucd_tlv_t_171_harq_ack_delay_dl_burst,
			{
				"HARQ ACK Delay for DL Burst", "wmx.ucd.harq_ack_delay_dl_burst",
				FT_UINT8, BASE_DEC|BASE_UNIT_STRING, UNS(&wimax_units_frame_offset), 0, NULL, HFILL
			}
		},
		{
			&hf_ucd_tlv_t_150_initial_ranging_codes,
			{
				"Initial Ranging Codes", "wmx.ucd.initial_ranging_codes",
				FT_UINT8, BASE_HEX, NULL, 0, NULL, HFILL
			}
		},
		{
			&hf_ucd_tlv_t_195_initial_ranging_interval,
			{
				"Number of Frames Between Initial Ranging Interval Allocation", "wmx.ucd.initial_ranging_interval",
				FT_INT8, BASE_DEC, NULL, 0, NULL, HFILL
			}
		},
		{
			&hf_ucd_invalid_tlv,
			{
				"Invalid TLV", "wmx.ucd.invalid_tlv",
				FT_BYTES, BASE_NONE, NULL, 0, NULL, HFILL
			}
		},
		{
			&hf_ucd_tlv_t_187_lower_bound_aas_preamble,
			{
				"Lower Bound AAS Preamble (in units of 0.25 dB)", "wmx.ucd.lower_bound_aas_preamble",
				FT_INT8, BASE_DEC, NULL, 0, NULL, HFILL
			}
		},
		{
			&hf_ucd_tlv_t_192_min_level_power_offset_adjustment,
			{
				"Minimum Level of Power Offset Adjustment (in units of 0.1 dB)", "wmx.ucd.min_level_power_offset_adjustment",
				FT_INT8, BASE_DEC, NULL, 0, NULL, HFILL
			}
		},
		{
			&hf_ucd_tlv_t_193_max_level_power_offset_adjustment,
			{
				"Maximum Level of Power Offset Adjustment (in units of 0.1 dB)", "wmx.ucd.max_level_power_offset_adjustment",
				FT_INT8, BASE_DEC, NULL, 0, NULL, HFILL
			}
		},
		{
			&hf_ucd_tlv_t_174_maximum_retransmission,
			{
				"Maximum Number of Retransmission in UL-HARQ", "wmx.ucd.max_number_of_retransmission_in_ul_harq",
				FT_UINT8, BASE_DEC, NULL, 0, NULL, HFILL
			}
		},
		{
			&hf_ucd_tlv_t_191_ms_specific_down_power_addjustment_step,
			{
				"MS-specific Down Power Offset Adjustment Step (in units of 0.01 dB)", "wmx.ucd.ms_specific_down_power_offset_adjustment_step",
				FT_UINT8, BASE_HEX, NULL, 0, NULL, HFILL
			}
		},
		{
			&hf_ucd_tlv_t_190_ms_specific_up_power_addjustment_step,
			{
				"MS-specific Up Power Offset Adjustment Step (in units of 0.01 dB)", "wmx.ucd.ms_specific_up_power_offset_adjustment_step",
				FT_UINT8, BASE_HEX, NULL, 0, NULL, HFILL
			}
		},
#if 0
		{
			&hf_ucd_tlv_t_197_normalized_cn_channel_sounding,
			{
				"Normalized C/N for Channel Sounding", "wmx.ucd.normalized_cn.channel_sounding",
				FT_UINT8, BASE_HEX, NULL, 0, NULL, HFILL
			}
		},
#endif
		{
			&hf_ucd_tlv_t_177_normalized_cn_override2,
			{
				"Normalized C/N Override 2", "wmx.ucd.normalized_cn.override_2",
				FT_STRING, BASE_NONE, NULL, 0, NULL, HFILL
			}
		},
		{
			&hf_ucd_tlv_t_177_normalized_cn_override2_first_line,
			{
				"Normalized C/N Value", "wmx.ucd.normalized_cn.override_first_line",
				FT_UINT8, BASE_HEX, NULL, 0, NULL, HFILL
			}
		},
		{
			&hf_ucd_tlv_t_177_normalized_cn_override2_list,
			{
				"Normalized C/N Value List", "wmx.ucd.normalized_cn.override_list",
				FT_STRING, BASE_NONE, NULL, 0, NULL, HFILL
			}
		},
		{
			&hf_ucd_tlv_t_158_optional_permutation_ul_allocated_subchannels_bitmap,
			{
				"Optional permutation UL allocated subchannels bitmap", "wmx.ucd.optional_permutation_ul_allocated_subchannels_bitmap",
				FT_BYTES, BASE_NONE, NULL, 0, NULL, HFILL
			}
		},
		{
			&hf_ucd_tlv_t_151_periodic_ranging_codes,
			{
				"Periodic Ranging Codes", "wmx.ucd.periodic_ranging_codes",
				FT_UINT8, BASE_HEX, NULL, 0, NULL, HFILL
			}
		},
		{
			&hf_ucd_tlv_t_156_permutation_base,
			{
				"Permutation Base", "wmx.ucd.permutation_base",
				FT_UINT8, BASE_DEC, NULL, 0, NULL, HFILL
			}
		},
		{
			&hf_ucd_ranging_req_size,
			{
				"Ranging Request Opportunity Size", "wmx.ucd.ranging_req_size",
				FT_UINT16, BASE_DEC|BASE_UNIT_STRING, UNS(&wimax_units_db), 0, NULL, HFILL
			}
		},
		{
			&hf_ucd_res_timeout,
			{
				"Contention-based Reservation Timeout", "wmx.ucd.res_timeout",
				FT_UINT8, BASE_DEC, NULL, 0, NULL, HFILL
			}
		},
		{
			&hf_ucd_tlv_t_170_safety_channel_retry_timer,
			{
				"Safety Channel Release Timer", "wmx.ucd.safety_channel_release_timer",
				FT_UINT8, BASE_HEX|BASE_UNIT_STRING, UNS(&wimax_units_frame_frames), 0, NULL, HFILL
			}
		},
		{
			&hf_ucd_tlv_t_176_size_of_cqich_id_field,
			{
				"Size of CQICH_ID Field", "wmx.ucd.size_of_cqich_id_field",
				FT_UINT8, BASE_DEC, VALS(vals_ucd_cqich_size), 0, NULL, HFILL
			}
		},
		{
			&hf_ucd_tlv_t_155_start_of_ranging_codes_group,
			{
				"Start of Ranging Codes Group", "wmx.ucd.start_of_ranging_codes_group",
				FT_UINT8, BASE_DEC, NULL, 0, NULL, HFILL
			}
		},
			{
			&hf_ucd_ul_allocated_subchannles_bitmap,
			{
				"UL Allocated Subchannels Bitmap", "wmx.ucd.subchan.bitmap",
				FT_BYTES, BASE_NONE, NULL, 0, NULL, HFILL
			}
		},
#if 0
		{
			&hf_ucd_subchan_codes,
			{
				"Periodic Ranging Codes", "wmx.ucd.subchan.codes",
				FT_UINT8, BASE_HEX, NULL, 0, NULL, HFILL
			}
		},
		{
			&hf_ucd_subchan_params_num_chan,
			{
				"Number of Subchannels", "wmx.ucd.subchan.num_chan",
				FT_UINT8, BASE_DEC, NULL, 0, NULL, HFILL
			}
		},
		{
			&hf_ucd_subchan_params_num_sym,
			{
				"Number of OFDMA Symbols", "wmx.ucd.subchan.num_sym",
				FT_UINT8, BASE_DEC, NULL, 0, NULL, HFILL
			}
		},
#endif
		{
			&hf_ucd_tlv_t_196_tx_power_report,
			{
				"Tx Power Report", "wmx.ucd.tx_power_report",
				FT_UINT24, BASE_HEX, NULL, 0, NULL, HFILL
			}
		},
		{
			&hf_ucd_tlv_t_196_tx_power_report_a_p_avg,
			{
				"A p_avg (in multiples of 1/16)", "wmx.ucd.tx_power_report.a_p_avg",
				FT_UINT8, BASE_DEC, NULL, 0xF0, NULL, HFILL
			}
		},
		{
			&hf_ucd_tlv_t_196_tx_power_report_a_p_avg_icqch,
			{
				"A p_avg (in multiples of 1/16) when ICQCH is allocated", "wmx.ucd.tx_power_report.a_p_avg_icqch",
				FT_UINT8, BASE_DEC, NULL, 0x0F, NULL, HFILL
			}
		},
		{
			&hf_ucd_tlv_t_196_tx_power_report_interval,
			{
				"Interval (expressed as power of 2)", "wmx.ucd.tx_power_report.interval",
				FT_UINT8, BASE_DEC, NULL, 0x0F, NULL, HFILL
			}
		},
		{
			&hf_ucd_tlv_t_196_tx_power_report_interval_icqch,
			{
				"Interval When ICQCH is Allocated (expressed as power of 2)", "wmx.ucd.tx_power_report.interval_icqch",
				FT_UINT8, BASE_DEC, NULL, 0xF0, NULL, HFILL
			}
		},
		{
			&hf_ucd_tlv_t_196_tx_power_report_threshold,
			{
				"Threshold", "wmx.ucd.tx_power_report.threshold",
				FT_UINT8, BASE_DEC, NULL, 0xF0, NULL, HFILL
			}
		},
		{
			&hf_ucd_tlv_t_196_tx_power_report_threshold_icqch,
			{
				"Threshold When ICQCH is Allocated to SS (in dB)", "wmx.ucd.tx_power_report.threshold_icqch",
				FT_UINT8, BASE_DEC, NULL, 0x0F, NULL, HFILL
			}
		},
#if 0
		{
			&hf_ucd_unknown_type,
			{
				"Unknown UCD Type", "wmx.ucd.unknown_tlv_type",
				FT_BYTES, BASE_NONE, NULL, 0, NULL, HFILL
			}
		},
#endif
		{
			&hf_ucd_tlv_t_202_uplink_burst_profile_for_multiple_fec_types,
			{
				"Uplink Burst Profile for Multiple FEC Types", "wmx.ucd.uplink_burst_profile.multiple_fec_types",
				FT_UINT8, BASE_HEX, NULL, 0, NULL, HFILL
			}
		},
		{
			&hf_ucd_tlv_t_203_ul_pusc_subchannel_rotation,
			{
				"Uplink PUSC Subchannel Rotation", "wmx.ucd.uplink_burst_profile.ul_pusc_subchannel_rotation",
				FT_UINT8, BASE_HEX, NULL, 0, NULL, HFILL
			}
		},
		{
			&hf_ucd_tlv_t_205_relative_power_offset_ul_harq_burst,
			{
				"Relative Power Offset UL HARQ Burst", "wmx.ucd.uplink_burst_profile.relative_power_offset_ul_harq_burst",
				FT_UINT8, BASE_HEX, NULL, 0, NULL, HFILL
			}
		},
		{
			&hf_ucd_tlv_t_206_relative_power_offset_ul_burst_containing_mac_mgmt_msg,
			{
				"Relative Power Offset UL Burst Containing MAC Mgmt Msg", "wmx.ucd.uplink_burst_profile.relative_power_offset_ul_burst_mac_mgmt_msg",
				FT_UINT8, BASE_HEX, NULL, 0, NULL, HFILL
			}
		},
		{
			&hf_ucd_tlv_t_207_ul_initial_transmit_timing,
			{
				"UL Initial Transmit Timing", "wmx.ucd.uplink_burst_profile.ul_initial_transmit_timing",
				FT_UINT8, BASE_HEX, NULL, 0, NULL, HFILL
			}
		},
		{
			&hf_ucd_tlv_t_210_fast_feedback_region,
			{
				"Fast Feedback Region", "wmx.ucd.uplink_burst_profile.fast_feedback_region",
				FT_BYTES, BASE_NONE, NULL, 0, NULL, HFILL
			}
		},
		{
			&hf_ucd_tlv_t_211_harq_ack_region,
			{
				"HARQ ACK Region", "wmx.ucd.uplink_burst_profile.harq_ack_region",
				FT_BYTES, BASE_NONE, NULL, 0, NULL, HFILL
			}
		},
		{
			&hf_ucd_tlv_t_212_ranging_region,
			{
				"Ranging Region", "wmx.ucd.uplink_burst_profile.ranging_region",
				FT_BYTES, BASE_NONE, NULL, 0, NULL, HFILL
			}
		},
		{
			&hf_ucd_tlv_t_213_sounding_region,
			{
				"Sounding Region", "wmx.ucd.uplink_burst_profile.sounding_region",
				FT_BYTES, BASE_NONE, NULL, 0, NULL, HFILL
			}
		},
		{
			&hf_ucd_tlv_t_186_upper_bound_aas_preamble,
			{
				"Upper Bound AAS Preamble (in units of 0.25 dB)", "wmx.ucd.upper_bound_aas_preamble",
				FT_INT8, BASE_DEC, NULL, 0, NULL, HFILL
			}
		},
		{
			&hf_ucd_tlv_t_189_use_cqich_indication_flag,
			{
				"Use CQICH Indication Flag", "wmx.ucd.use_cqich_indication_flag",
				FT_UINT8, BASE_HEX, NULL, 0, NULL, HFILL
			}
		},
		{
			&hf_ucd_ho_ranging_start,
			{
				"Initial Backoff Window Size for MS Performing Initial During Handover Process", "wmx.ucd.ho_ranging_start",
				FT_UINT8, BASE_DEC, NULL, 0, NULL, HFILL
			}
		},
		{
			&hf_ucd_ho_ranging_end,
			{
				"Final Backoff Window Size for MS Performing Initial During Handover Process", "wmx.ucd.ho_ranging_end",
				FT_UINT8, BASE_DEC, NULL, 0, NULL, HFILL
			}
		},
		{
			&hf_ucd_initial_range_backoff_start,
			{
				"Initial Ranging Backoff Start", "wmx.ucd.initial_range_backoff_start",
				FT_UINT8, BASE_DEC, NULL, 0, NULL, HFILL
			}
		},
		{
			&hf_ucd_initial_range_backoff_end,
			{
				"Initial Ranging Backoff End", "wmx.ucd.initial_range_backoff_end",
				FT_UINT8, BASE_DEC, NULL, 0, NULL, HFILL
			}
		},
		{
			&hf_ucd_bandwidth_backoff_start,
			{
				"Bandwidth Request Backoff Start", "wmx.ucd.bandwidth_backoff_start",
				FT_UINT8, BASE_DEC, NULL, 0, NULL, HFILL
			}
		},
		{
			&hf_ucd_bandwidth_backoff_end,
			{
				"Bandwidth Request Backoff End", "wmx.ucd.bandwidth_backoff_end",
				FT_UINT8, BASE_DEC, NULL, 0, NULL, HFILL
			}
		},
		{
			&hf_ucd_periodic_ranging_backoff_start,
			{
				"Periodic Ranging Backoff Start", "wmx.ucd.periodic_ranging_backoff_start",
				FT_UINT8, BASE_DEC, NULL, 0, NULL, HFILL
			}
		},
		{
			&hf_ucd_periodic_ranging_backoff_end,
			{
				"Periodic Ranging Backoff End", "wmx.ucd.periodic_ranging_backoff_end",
				FT_UINT8, BASE_DEC, NULL, 0, NULL, HFILL
			}
		},
		{
			&hf_ucd_config_change_count,
			{
				"Configuration Change Count", "wmx.ucd.config_change_count",
				FT_UINT8, BASE_DEC, NULL, 0, NULL, HFILL
			}
		},
		{
			&hf_ucd_ranging_backoff_start,
			{
				"Ranging Backoff Start", "wmx.ucd.ranging_backoff_start",
				FT_UINT8, BASE_DEC, NULL, 0, NULL, HFILL
			}
		},
		{
			&hf_ucd_ranging_backoff_end,
			{
				"Ranging Backoff End", "wmx.ucd.ranging_backoff_end",
				FT_UINT8, BASE_DEC, NULL, 0, NULL, HFILL
			}
		},
		{
			&hf_ucd_request_backoff_start,
			{
				"Request Backoff Start", "wmx.ucd.request_backoff_start",
				FT_UINT8, BASE_DEC, NULL, 0, NULL, HFILL
			}
		},
		{
			&hf_ucd_request_backoff_end,
			{
				"Request Backoff End", "wmx.ucd.request_backoff_end",
				FT_UINT8, BASE_DEC, NULL, 0, NULL, HFILL
			}
		},
	};

	/* Setup protocol subtree array */
	static int *ett[] =
		{
			&ett_mac_mgmt_msg_ucd_decoder,
		};

	proto_mac_mgmt_msg_ucd_decoder = proto_register_protocol (
		"WiMax UCD Messages", /* name       */
		"WiMax UCD",     /* short name */
		"wmx.ucd"                  /* abbrev     */
		);

	proto_register_field_array(proto_mac_mgmt_msg_ucd_decoder, hf, array_length(hf));
	proto_register_subtree_array(ett, array_length(ett));
	ucd_handle = register_dissector("mac_mgmt_msg_ucd_handler", dissect_mac_mgmt_msg_ucd_decoder, proto_mac_mgmt_msg_ucd_decoder);
}

void proto_reg_handoff_mac_mgmt_msg_ucd(void)
{
	dissector_add_uint("wmx.mgmtmsg", MAC_MGMT_MSG_UCD, ucd_handle);
}

/*
 * Editor modelines  -  https://www.wireshark.org/tools/modelines.html
 *
 * Local variables:
 * c-basic-offset: 8
 * tab-width: 8
 * indent-tabs-mode: t
 * End:
 *
 * vi: set shiftwidth=8 tabstop=8 noexpandtab:
 * :indentSize=8:tabSize=8:noTabs=false:
 */
