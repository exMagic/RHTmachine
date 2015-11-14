/* Command line parser for data node
  
   Copyright (c) 2015 Andrey Chilikin (https://github.com/achilikin)

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
   THE SOFTWARE.
   https://opensource.org/licenses/MIT
*/
#include <Arduino.h>

#include "main.h"
#include "serial.h"
#include "serial_cli.h"

// list of supported commands 
static const char usage[] PROGMEM =
	"  mem\n"             // show available memory
	"  status\n"          // show current settings
	"  print history\n"   // print temperature and humidity history(12/24h)
	"  config on|off\n"   // turn on configuration mode
	"  set gauge 0-255\n" // calbrate gauge in configuration mode
	"  echo rht|thist|verbose on|off"; // debug echo on/off
static const char ps_on[] PROGMEM = "on";
static const char ps_off[] PROGMEM = "off";
static const char ps_config[] PROGMEM = "config";

const char *is_on(uint8_t bit)
{
	if (bit)
		return "on";
	return "off";
}

void print_status(uint8_t echo_only)
{
	if (!echo_only) {
		printf_P(PSTR("Uptime: "));
		print_time(uptime);
		printf_P(PSTR("\nmachine   : "));
		printf_P((pins & CD_ATTACHED) ? PSTR("at") : PSTR("de"));
		printf_P(PSTR("tached\n"));
		printf_P(PSTR("display   : "));
		if (pins & DISP_RH)
			printf_P(PSTR("humidity"));
		else
			printf_P(PSTR("temperature"));
		printf_P(PSTR("\n24 hour   : %s\n"), is_on(pins & HIST_24H));
		printf_P(PSTR("config mod: %s\n"), is_on(flags & CONFIG_MODE));
	}
	printf_P(PSTR("verbose   : %s\n"), is_on(flags & VERBOSE_MODE));
	printf_P(PSTR("thist echo: %s\n"), is_on(flags & ECHO_THIST));
	printf_P(PSTR("rht echo  : %s\n"), is_on(flags & ECHO_RHT));
}

int8_t cli_proc(char *buf, void *ptr)
{
	char *arg;
	char cmd[CMD_LEN + 1];

	memcpy(cmd, buf, sizeof(cmd));
	arg = get_arg(cmd);

	if (str_is(cmd, PSTR("help"))) {
		puts_P(usage);
		return 0;
	}

	if (str_is(cmd, ps_config)) {
		if (str_is(arg, ps_on))
			flags |= CONFIG_MODE;
		else if (str_is(arg, ps_off))
			flags &= ~CONFIG_MODE;
		printf_P(ps_config);
		printf_P(PSTR(" is %s\n"), is_on(flags & CONFIG_MODE));
		return 0;
	}

	if (str_is(cmd, PSTR("status"))) {
		print_status(0);
		return 0;
	}

	if (str_is(cmd, PSTR("print"))) {
		if (!str_is(arg, PSTR("history")))
			return CLI_EARG;
		print_hist();
		return 0;
	}

	if (str_is(cmd, PSTR("set"))) {
		char *val = get_arg(arg);
		if (str_is(arg, PSTR("gauge"))) {
			if (flags & CONFIG_MODE) {
				uint8_t pwm = atoi(val);
				set_gauge(pwm);
			}
			return 0;
		}
		return CLI_EARG;
	}

	if (str_is(cmd, PSTR("echo"))) {
		if (*arg == '\0') {
			print_status(1);
			return 0;
		}
		char *on = get_arg(arg);
		if (str_is(arg, ps_off)) {
			flags &= ECHO_RHT | ECHO_THIST | VERBOSE_MODE;
			return 0;
		}
		uint8_t set = 0;
		if (str_is(arg, PSTR("rht")))
			set = ECHO_RHT;
		if (str_is(arg, PSTR("thist")))
			set = ECHO_THIST;
		if (str_is(arg, PSTR("verbose")))
			set = VERBOSE_MODE;
		if (!set)
			return CLI_EARG;
		if (str_is(on, ps_on))
			flags |= set;
		else if (str_is(on, ps_off))
			flags &= ~set;
		else
			return CLI_EARG;
		print_status(1);
		return 0;
	}

	if (str_is(cmd, PSTR("mem"))) {
		printf_P(PSTR("memory %u\n"), free_mem());
		return 0;
	}
	return CLI_ENOTSUP;
}