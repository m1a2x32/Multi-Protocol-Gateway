/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zephyr/net/net_core.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/wifi_mgmt.h>

#define MYSSID "Europalaan-4C"
#define MYPSK "56434323"

LOG_MODULE_REGISTER(mainThread, CONFIG_LOG_DEFAULT_LEVEL);

#define WIFI_MGMT_EVENTS ( 	NET_EVENT_WIFI_SCAN_RESULT | 		\
							NET_EVENT_WIFI_SCAN_DONE  | 		\
							NET_EVENT_WIFI_CONNECT_RESULT |		\
							NET_EVENT_WIFI_DISCONNECT_RESULT)


static uint32_t scan_result;
static struct net_mgmt_event_callback wifi_sta_mgmt_cb;
struct wifi_connect_req_params cnx_params;

static int wifi_connect()
{
	struct net_if *iface = net_if_get_default();

	if (net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &cnx_params, sizeof(struct wifi_connect_req_params))) {
		LOG_ERR("Error connecting to wifi station");
		return -ENOEXEC;
	}

	LOG_INF("Initiating connection\n");
	return 0;
}

static int wifi_scan()
{
	struct net_if *iface = net_if_get_default();

	if (net_mgmt(NET_REQUEST_WIFI_SCAN, iface, NULL, 0)) {
		LOG_ERR("Scan request failed");
		return -ENOEXEC;
	}

	LOG_INF("Scan Requested: \n");
	return 0;
}

static void handle_wifi_scan_result(struct net_mgmt_event_callback *cb)
{
	const struct wifi_scan_result *entry =
		(const struct wifi_scan_result *)cb->info;
	uint8_t mac_string_buf[sizeof("xx:xx:xx:xx:xx:xx")];
	scan_result++;

	if (scan_result == 1U) {
		printf("\nNum | SSID | Len | Band | RSSI | Security\n");
	}

	printf("%-4d | %-32s %-5u | %-4u (%-6s) | %-4d | %-15s\n",
	      scan_result, entry->ssid, entry->ssid_length, entry->channel,
	      wifi_band_txt(entry->band),
	      entry->rssi,
	      wifi_security_txt(entry->security));	

	if (strcmp(MYSSID, entry->ssid) == 0)
	{
		cnx_params.timeout = SYS_FOREVER_MS;
		cnx_params.ssid = entry->ssid; // setup kconf file
		cnx_params.ssid_length = strlen(MYSSID);
		cnx_params.channel = entry->channel;
		cnx_params.security = entry->security;
		if (cnx_params.security!=WIFI_SECURITY_TYPE_NONE)
		{
			cnx_params.psk = MYPSK;
			cnx_params.psk_length = strlen(MYPSK);
		}
		cnx_params.mfp = entry->mfp;
	}
}

static void handle_wifi_scan_done(struct net_mgmt_event_callback *cb)
{
	const struct wifi_status *status =
		(const struct wifi_status *)cb->info;

	if (status->status) {
		printf("Scan request failed\n");
	} else {
		printf("Scan request done\n Connecting to %s\n", MYSSID);
		wifi_connect();
	}
	scan_result = 0U;
}

static void handle_wifi_connect_result(struct net_mgmt_event_callback *cb)
{
	const struct wifi_status *status =
		(const struct wifi_status *) cb->info;

	if (status->status) {
		LOG_ERR("Connection request failed (%d)", status->status);
	} else {
		LOG_INF("Connected");
	}
}

static void handle_wifi_disconnect_result(struct net_mgmt_event_callback *cb)
{
	const struct wifi_status *status =
		(const struct wifi_status *) cb->info;

	LOG_ERR("Disconnected (%d)\n Reconnecting to %s", status->status, MYSSID);
	wifi_connect();
}

static void wifi_mgmt_event_handler(struct net_mgmt_event_callback *cb,
				    uint32_t mgmt_event, struct net_if *iface)
{
	switch (mgmt_event) {
	case NET_EVENT_WIFI_SCAN_RESULT:
		handle_wifi_scan_result(cb);
		break;
	case NET_EVENT_WIFI_SCAN_DONE:
		handle_wifi_scan_done(cb);
		break;
	case NET_EVENT_WIFI_CONNECT_RESULT:
		handle_wifi_connect_result(cb);
		break;
	case NET_EVENT_WIFI_DISCONNECT_RESULT:
		handle_wifi_disconnect_result(cb);
		break;
	default:
		break;
	}
}

void main(void)
{
	net_mgmt_init_event_callback(&wifi_sta_mgmt_cb,
				wifi_mgmt_event_handler,
				WIFI_MGMT_EVENTS);
	net_mgmt_add_event_callback(&wifi_sta_mgmt_cb);

	wifi_scan();
}
