/*
 * Copyright (c) 2013-2015, Dalian Futures Information Technology Co., Ltd.
 *
 * Bo Wang <futurewb at dce dot com dot cn>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "macros.h"
#include "mem.h"
#include "dstr.h"
#include "logger.h"
#include "config.h"
#include "module.h"
#include "utilities.h"
#include "basics.h"
#include "sec.h"

/* FIXME */
static char *app = "sec";
static char *desc = "Security level1 API";
static sec_mdspi_t *mdspi;
static sec_mdapi_t *mdapi;
static struct config *cfg;
static const char *front_ip, *front_port;
static const char *stock_userid, *stock_passwd, *stock_reqid = "1", *stock_contracts;
static const char *sop_userid, *sop_passwd, *sop_reqid = "1", *sop_contracts;

static inline void load_config(void) {
	/* FIXME */
	if ((cfg = config_load("/etc/xcb/sec.conf"))) {
		char *cat = category_browse(cfg, NULL);

		while (cat) {
			if (!strcasecmp(cat, "general")) {
				struct variable *var = variable_browse(cfg, cat);

				while (var) {
					if (!strcasecmp(var->name, "front_ip")) {
						if (strcasecmp(var->value, ""))
							front_ip = var->value;
					} else if (!strcasecmp(var->name, "front_port")) {
						if (strcasecmp(var->value, ""))
							front_port = var->value;
					} else if (!strcasecmp(var->name, "stock_userid")) {
						if (strcasecmp(var->value, ""))
							stock_userid = var->value;
					} else if (!strcasecmp(var->name, "stock_passwd")) {
						if (strcasecmp(var->value, ""))
							stock_passwd = var->value;
					} else if (!strcasecmp(var->name, "stock_reqid")) {
						if (strcasecmp(var->value, ""))
							stock_reqid = var->value;
					} else if (!strcasecmp(var->name, "stock_contracts")) {
						if (strcasecmp(var->value, ""))
							stock_contracts = var->value;
					} else if (!strcasecmp(var->name, "sop_userid")) {
						if (strcasecmp(var->value, ""))
							sop_userid = var->value;
					} else if (!strcasecmp(var->name, "sop_passwd")) {
						if (strcasecmp(var->value, ""))
							sop_passwd = var->value;
					} else if (!strcasecmp(var->name, "sop_reqid")) {
						if (strcasecmp(var->value, ""))
							sop_reqid = var->value;
					} else if (!strcasecmp(var->name, "sop_contracts")) {
						if (strcasecmp(var->value, ""))
							sop_contracts = var->value;
					} else
						xcb_log(XCB_LOG_WARNING, "Unknown variable '%s' in "
							"category '%s' of sec.conf", var->name, cat);
					var = var->next;
				}
			}
			cat = category_browse(cfg, cat);
		}
	}
}

static void on_front_connected(void) {
	struct DFITCSECReqUserLoginField stock_req, sop_req;
	int stock_res, sop_res;

	memset(&stock_req, 0, sizeof stock_req);
	memset(&sop_req, 0, sizeof sop_req);
	/* FIXME */
	strcpy(stock_req.accountID, stock_userid);
	strcpy(stock_req.passWord, stock_passwd);
	stock_req.requestID = atoi(stock_reqid);
	strcpy(sop_req.accountID, sop_userid);
	strcpy(sop_req.passWord, sop_passwd);
	sop_req.requestID = atoi(sop_reqid);
	stock_res = sec_mdapi_stock_user_login(mdapi, &stock_req);
	xcb_log(XCB_LOG_NOTICE, "Stock login %s for user '%s'",
		stock_res == 0 ? "succeeded" : "failed", stock_userid);
	sop_res = sec_mdapi_sop_user_login(mdapi, &sop_req);
	xcb_log(XCB_LOG_NOTICE, "Sop login %s for user '%s'", sop_res == 0 ? "succeeded" : "failed", sop_userid);
}

static void on_front_disconnected(int reason) {
	xcb_log(XCB_LOG_NOTICE, "Front disconnected: reason = '%d'", reason);
}

static void on_stock_user_login(struct DFITCSECRspUserLoginField *userlogin, struct DFITCSECRspInfoField *rspinfo) {
	/* FIXME */
	if (rspinfo == NULL)
		if (stock_contracts) {
			dstr *fields = NULL;
			int nfield = 0, i;

			fields = dstr_split_len(stock_contracts, strlen(stock_contracts), ",", 1, &nfield);
			for (i = 0; i < nfield; ++i)
				if (sec_mdapi_stock_subscribe_market_data(mdapi, &fields[i], 1, atoi(stock_reqid)) == 0)
					xcb_log(XCB_LOG_INFO, "Stock subscribe contract '%s' succeeded", fields[i]);
				else
					xcb_log(XCB_LOG_INFO, "Stock subscribe contract '%s' failed", fields[i]);
			dstr_free_tokens(fields, nfield);
		}
}

static void on_stock_deep_market_data(struct DFITCStockDepthMarketDataField *deepmd) {
	Quote *quote;
	char contract[256];

	if (deepmd == NULL)
		return;
	if (NEW0(quote)) {
		/* FIXME */
		quote->thyquote.m_nLen   = sizeof (THYQuote);
		quote->thyquote.m_nTime  = atoi(deepmd->sharedDataField.updateTime);
		strcpy(quote->thyquote.m_cJYS, deepmd->staticDataField.exchangeID);
		strcpy(contract, deepmd->staticDataField.exchangeID);
		strcat(contract, deepmd->staticDataField.securityID);
		strncpy(quote->thyquote.m_cHYDM, contract, sizeof quote->thyquote.m_cHYDM - 1);
		quote->thyquote.m_bTPBZ  = 0;
		quote->thyquote.m_dZJSJ  = 0;
		quote->thyquote.m_dJJSJ  = 0;
		quote->thyquote.m_dCJJJ  = 0;
		quote->thyquote.m_dZSP   = deepmd->staticDataField.preClosePrice;
		quote->thyquote.m_dJSP   = 0;
		quote->thyquote.m_dJKP   = deepmd->staticDataField.openPrice;
		quote->thyquote.m_nZCCL  = 0;
		quote->thyquote.m_nCCL   = 0;
		quote->thyquote.m_dZXJ   = deepmd->sharedDataField.latestPrice;
		quote->thyquote.m_nCJSL  = deepmd->sharedDataField.tradeQty;
		quote->thyquote.m_dCJJE  = deepmd->sharedDataField.turnover;
		quote->thyquote.m_dZGBJ  = 0;
		quote->thyquote.m_dZDBJ  = 0;
		quote->thyquote.m_dZGJ   = deepmd->sharedDataField.highestPrice;
		quote->thyquote.m_dZDJ   = deepmd->sharedDataField.lowestPrice;
		quote->thyquote.m_dZXSD  = 0;
		quote->thyquote.m_dJXSD  = 0;
		quote->thyquote.m_dMRJG1 = deepmd->sharedDataField.bidPrice1;
		quote->thyquote.m_dMCJG1 = deepmd->sharedDataField.askPrice1;
		quote->thyquote.m_nMRSL1 = deepmd->sharedDataField.bidQty1;
		quote->thyquote.m_nMCSL1 = deepmd->sharedDataField.askQty1;
		quote->thyquote.m_dMRJG2 = deepmd->sharedDataField.bidPrice2;
		quote->thyquote.m_dMCJG2 = deepmd->sharedDataField.askPrice2;
		quote->thyquote.m_nMRSL2 = deepmd->sharedDataField.bidQty2;
		quote->thyquote.m_nMCSL2 = deepmd->sharedDataField.askQty2;
		quote->thyquote.m_dMRJG3 = deepmd->sharedDataField.bidPrice3;
		quote->thyquote.m_dMCJG3 = deepmd->sharedDataField.askPrice3;
		quote->thyquote.m_nMRSL3 = deepmd->sharedDataField.bidQty3;
		quote->thyquote.m_nMCSL3 = deepmd->sharedDataField.askQty3;
		quote->thyquote.m_dMRJG4 = deepmd->sharedDataField.bidPrice4;
		quote->thyquote.m_dMCJG4 = deepmd->sharedDataField.askPrice4;
		quote->thyquote.m_nMRSL4 = deepmd->sharedDataField.bidQty4;
		quote->thyquote.m_nMCSL4 = deepmd->sharedDataField.askQty4;
		quote->thyquote.m_dMRJG5 = deepmd->sharedDataField.bidPrice5;
		quote->thyquote.m_dMCJG5 = deepmd->sharedDataField.askPrice5;
		quote->thyquote.m_nMRSL5 = deepmd->sharedDataField.bidQty5;
		quote->thyquote.m_nMCSL5 = deepmd->sharedDataField.askQty5;
		process_quote(quote);
	} else
		xcb_log(XCB_LOG_WARNING, "Error allocating memory for quote");
}

static void on_sop_user_login(struct DFITCSECRspUserLoginField *userlogin, struct DFITCSECRspInfoField *rspinfo) {
	/* FIXME */
	if (rspinfo == NULL)
		if (sop_contracts) {
			dstr *fields = NULL;
			int nfield = 0, i;

			fields = dstr_split_len(sop_contracts, strlen(sop_contracts), ",", 1, &nfield);
			for (i = 0; i < nfield; ++i)
				if (sec_mdapi_sop_subscribe_market_data(mdapi, &fields[i], 1, atoi(sop_reqid)) == 0)
					xcb_log(XCB_LOG_INFO, "Sop subscribe contract '%s' succeeded", fields[i]);
				else
					xcb_log(XCB_LOG_INFO, "Sop Subscribe contract '%s' failed", fields[i]);
			dstr_free_tokens(fields, nfield);
		}
}

static void on_sop_deep_market_data(struct DFITCSOPDepthMarketDataField *deepmd) {
	Quote *quote;

	if (deepmd == NULL)
		return;
	if (NEW0(quote)) {
		/* FIXME */
		quote->thyquote.m_nLen   = sizeof (THYQuote);
		quote->thyquote.m_nTime  = atoi(deepmd->sharedDataField.updateTime);
		strcpy(quote->thyquote.m_cJYS, deepmd->staticDataField.exchangeID);
		strcpy(quote->thyquote.m_cHYDM, deepmd->specificDataField.contractID);
		quote->thyquote.m_bTPBZ  = 0;
		quote->thyquote.m_dZJSJ  = deepmd->specificDataField.preSettlePrice;
		quote->thyquote.m_dJJSJ  = deepmd->specificDataField.settlePrice;
		quote->thyquote.m_dCJJJ  = 0;
		quote->thyquote.m_dZSP   = deepmd->staticDataField.preClosePrice;
		quote->thyquote.m_dJSP   = 0;
		quote->thyquote.m_dJKP   = deepmd->staticDataField.openPrice;
		quote->thyquote.m_nZCCL  = 0;
		quote->thyquote.m_nCCL   = deepmd->specificDataField.positionQty;
		quote->thyquote.m_dZXJ   = deepmd->sharedDataField.latestPrice;
		quote->thyquote.m_nCJSL  = deepmd->sharedDataField.tradeQty;
		quote->thyquote.m_dCJJE  = deepmd->sharedDataField.turnover;
		quote->thyquote.m_dZGBJ  = 0;
		quote->thyquote.m_dZDBJ  = 0;
		quote->thyquote.m_dZGJ   = deepmd->sharedDataField.highestPrice;
		quote->thyquote.m_dZDJ   = deepmd->sharedDataField.lowestPrice;
		quote->thyquote.m_dZXSD  = 0;
		quote->thyquote.m_dJXSD  = 0;
		quote->thyquote.m_dMRJG1 = deepmd->sharedDataField.bidPrice1;
		quote->thyquote.m_dMCJG1 = deepmd->sharedDataField.askPrice1;
		quote->thyquote.m_nMRSL1 = deepmd->sharedDataField.bidQty1;
		quote->thyquote.m_nMCSL1 = deepmd->sharedDataField.askQty1;
		quote->thyquote.m_dMRJG2 = deepmd->sharedDataField.bidPrice2;
		quote->thyquote.m_dMCJG2 = deepmd->sharedDataField.askPrice2;
		quote->thyquote.m_nMRSL2 = deepmd->sharedDataField.bidQty2;
		quote->thyquote.m_nMCSL2 = deepmd->sharedDataField.askQty2;
		quote->thyquote.m_dMRJG3 = deepmd->sharedDataField.bidPrice3;
		quote->thyquote.m_dMCJG3 = deepmd->sharedDataField.askPrice3;
		quote->thyquote.m_nMRSL3 = deepmd->sharedDataField.bidQty3;
		quote->thyquote.m_nMCSL3 = deepmd->sharedDataField.askQty3;
		quote->thyquote.m_dMRJG4 = deepmd->sharedDataField.bidPrice4;
		quote->thyquote.m_dMCJG4 = deepmd->sharedDataField.askPrice4;
		quote->thyquote.m_nMRSL4 = deepmd->sharedDataField.bidQty4;
		quote->thyquote.m_nMCSL4 = deepmd->sharedDataField.askQty4;
		quote->thyquote.m_dMRJG5 = deepmd->sharedDataField.bidPrice5;
		quote->thyquote.m_dMCJG5 = deepmd->sharedDataField.askPrice5;
		quote->thyquote.m_nMRSL5 = deepmd->sharedDataField.bidQty5;
		quote->thyquote.m_nMCSL5 = deepmd->sharedDataField.askQty5;
		process_quote(quote);
	} else
		xcb_log(XCB_LOG_WARNING, "Error allocating memory for quote");
}

static int sec_exec(void *data, void *data2) {
	RAII_VAR(struct msg *, msg, (struct msg *)data, msg_decr);

	/* do nothing */
	return 0;
}

static int load_module(void) {
	char front[1024];

	load_config();
	if (front_ip == NULL || front_port == NULL ||
		stock_userid == NULL || stock_passwd == NULL || sop_userid == NULL || sop_passwd == NULL) {
		xcb_log(XCB_LOG_ERROR, "protocol, front_ip, front_port, userid or passwd can't be empty");
		return MODULE_LOAD_FAILURE;
	}
	mdspi = sec_mdspi_create();
	sec_mdspi_on_front_connected(mdspi, on_front_connected);
	sec_mdspi_on_front_disconnected(mdspi, on_front_disconnected);
	sec_mdspi_on_stock_user_login(mdspi, on_stock_user_login);
	sec_mdspi_on_stock_deep_market_data(mdspi, on_stock_deep_market_data);
	sec_mdspi_on_sop_user_login(mdspi, on_sop_user_login);
	sec_mdspi_on_sop_deep_market_data(mdspi, on_sop_deep_market_data);
	mdapi = sec_mdapi_create();
	/* FIXME */
	snprintf(front, sizeof front, "tcp://%s:%s", front_ip, front_port);
	sec_mdapi_init(mdapi, front, mdspi);
	return register_application(app, sec_exec, desc, NULL, mod_info->self);
}

static int unload_module(void) {
	/* FIXME */
	sec_mdapi_release(mdapi);
	sec_mdspi_destroy(mdspi);
	config_destroy(cfg);
	return unregister_application(app);
}

static int reload_module(void) {
	/* FIXME */
	return MODULE_LOAD_SUCCESS;
}

MODULE_INFO(load_module, unload_module, reload_module, "Security level1 API application");

