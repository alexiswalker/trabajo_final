/**
 * This is a helper module for accessing and storage the different CPU registers.
 *
 * Copyright: © 2015-2017, Dan Printzell
 * License: $(LINK2 https://www.mozilla.org/en-US/MPL/2.0/, Mozilla Public License Version 2.0)
 *  (See accompanying file LICENSE)
 * Authors: $(LINK2 https://vild.io/, Dan Printzell)
 */
module arch.amd64.register;

import stl.address : VirtAddress;

private extern (C) VirtAddress cpuRetCR0() @safe; //XXX: @safe
private extern (C) VirtAddress cpuRetCR2() @safe; //XXX: @safe
private extern (C) VirtAddress cpuRetCR3() @safe; //XXX: @safe
private extern (C) VirtAddress cpuRetCR4() @safe; //XXX: @safe

@safe struct Registers {
align(1):
	VirtAddress r15, r14, r13, r12, r11, r10, r9, r8;
	VirtAddress rbp, rdi, rsi, rdx, rcx, rbx, rax;
	VirtAddress intNumber, errorCode;
	VirtAddress rip, cs, flags, rsp, ss;

	@property VirtAddress cr0() const {
		return cpuRetCR0();
	}

	@property VirtAddress cr2() const {
		return cpuRetCR2();
	}

	@property VirtAddress cr3() const {
		return cpuRetCR3();
	}

	@property VirtAddress cr4() const {
		return cpuRetCR4();
	}
}
