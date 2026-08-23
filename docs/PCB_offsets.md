Field	Offset	Description
pid	0x00	Process ID
name	0x08	Process name (32 bytes)
state	0x28	Process state
cr3	0x30	Page table root
entry_point	0x38	User entry point
kernel_stack_phys	0x40	Kernel stack physical
kernel_stack_virt	0x48	Kernel stack virtual
kernel_stack_top	0x50	Kernel stack top
user_stack_phys	0x58	User stack physical
user_stack_virt	0x60	User stack virtual
user_stack_top	0x68	User stack top ← This is what we need!
r15-r8, rbp, etc.	0x70+	Context registers
