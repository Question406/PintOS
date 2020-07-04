
loader.out:     file format elf32-i386


Disassembly of section .text:

00007c00 <read_mbr-0x1e>:

# Set up segment registers.
# Set stack to grow downward from 60 kB (after boot, the kernel
# continues to use this stack for its initial thread).

	sub %ax, %ax
    7c00:	29 c0                	sub    %eax,%eax
	mov %ax, %ds
    7c02:	8e d8                	mov    %eax,%ds
	mov %ax, %ss
    7c04:	8e d0                	mov    %eax,%ss
	mov $0xf000, %esp
    7c06:	66 bc 00 f0          	mov    $0xf000,%sp
    7c0a:	00 00                	add    %al,(%eax)

# Configure serial port so we can report progress without connected VGA.
# See [IntrList] for details.
	sub %dx, %dx			# Serial port 0.
    7c0c:	29 d2                	sub    %edx,%edx
	mov $0xe3, %al			# 9600 bps, N-8-1.
    7c0e:	b0 e3                	mov    $0xe3,%al
					# AH is already 0 (Initialize Port).
	int $0x14			# Destroys AX.
    7c10:	cd 14                	int    $0x14

	call puts
    7c12:	e8 d2 00 50 69       	call   69507ce9 <__bss_start+0x694ffee9>
    7c17:	6e                   	outsb  %ds:(%esi),(%dx)
    7c18:	74 6f                	je     7c89 <load_kernel+0xb>
    7c1a:	73 00                	jae    7c1c <read_mbr-0x2>
####
#### We print out status messages to show the disk and partition being
#### scanned, e.g. hda1234 as we scan four partitions on the first
#### hard disk.

	mov $0x80, %dl			# Hard disk 0.
    7c1c:	b2 80                	mov    $0x80,%dl

00007c1e <read_mbr>:
read_mbr:
	sub %ebx, %ebx			# Sector 0.
    7c1e:	66 29 db             	sub    %bx,%bx
	mov $0x2000, %ax		# Use 0x20000 for buffer.
    7c21:	b8 00 20 8e c0       	mov    $0xc08e2000,%eax
	mov %ax, %es
	call read_sector
    7c26:	e8 f6 00 72 42       	call   42727d21 <__bss_start+0x4271ff21>
	jc no_such_drive

	# Print hd[a-z].
	call puts
    7c2b:	e8 b9 00 20 68       	call   68207ce9 <__bss_start+0x681ffee9>
    7c30:	64 00 88 d0 04 e1 e8 	add    %cl,%fs:-0x171efb30(%eax)
	.string " hd"
	mov %dl, %al
	add $'a' - 0x80, %al
	call putc
    7c37:	c6 00 26             	movb   $0x26,(%eax)

	# Check for MBR signature--if not present, it's not a
	# partitioned hard disk.
	cmpw $0xaa55, %es:510
    7c3a:	81 3e fe 01 55 aa    	cmpl   $0xaa5501fe,(%esi)
	jne next_drive
    7c40:	75 27                	jne    7c69 <next_drive>

	mov $446, %si			# Offset of partition table entry 1.
    7c42:	be be 01 b0 31       	mov    $0x31b001be,%esi

00007c47 <check_partition>:
	mov $'1', %al
check_partition:
	# Is it an unused partition?
	cmpl $0, %es:(%si)
    7c47:	26 66 83 3c 00 74    	cmpw   $0x74,%es:(%eax,%eax,1)
	je next_partition
    7c4d:	10 e8                	adc    %ch,%al

	# Print [1-4].
	call putc
    7c4f:	ae                   	scas   %es:(%edi),%al
    7c50:	00 26                	add    %ah,(%esi)

	# Is it a Pintos kernel partition?
	cmpb $0x20, %es:4(%si)
    7c52:	80 7c 04 20 75       	cmpb   $0x75,0x20(%esp,%eax,1)
	jne next_partition
    7c57:	06                   	push   %es

	# Is it a bootable partition?
	cmpb $0x80, %es:(%si)
    7c58:	26 80 3c 80 74       	cmpb   $0x74,%es:(%eax,%eax,4)
	je load_kernel
    7c5d:	20                   	.byte 0x20

00007c5e <next_partition>:

next_partition:
	# No match for this partition, go on to the next one.
	add $16, %si			# Offset to next partition table entry.
    7c5e:	83 c6 10             	add    $0x10,%esi
	inc %al
    7c61:	fe c0                	inc    %al
	cmp $510, %si
    7c63:	81 fe fe 01 72 de    	cmp    $0xde7201fe,%esi

00007c69 <next_drive>:
	jb check_partition

next_drive:
	# No match on this drive, go on to the next one.
	inc %dl
    7c69:	fe c2                	inc    %dl
	jnc read_mbr
    7c6b:	73 b1                	jae    7c1e <read_mbr>

00007c6d <no_boot_partition>:

no_such_drive:
no_boot_partition:
	# Didn't find a Pintos kernel partition anywhere, give up.
	call puts
    7c6d:	e8 77 00 0d 4e       	call   4e0d7ce9 <__bss_start+0x4e0cfee9>
    7c72:	6f                   	outsl  %ds:(%esi),(%dx)
    7c73:	74 20                	je     7c95 <load_kernel+0x17>
    7c75:	66 6f                	outsw  %ds:(%esi),(%dx)
    7c77:	75 6e                	jne    7ce7 <puts>
    7c79:	64                   	fs
    7c7a:	0d                   	.byte 0xd
    7c7b:	00 cd                	add    %cl,%ch
	.string "\rNot found\r"

	# Notify BIOS that boot failed.  See [IntrList].
	int $0x18
    7c7d:	18                   	.byte 0x18

00007c7e <load_kernel>:
#### We found a kernel.  The kernel's drive is in DL.  The partition
#### table entry for the kernel's partition is at ES:SI.  Our job now
#### is to read the kernel from disk and jump to its start address.

load_kernel:
	call puts
    7c7e:	e8 66 00 0d 4c       	call   4c0d7ce9 <__bss_start+0x4c0cfee9>
    7c83:	6f                   	outsl  %ds:(%esi),(%dx)
    7c84:	61                   	popa   
    7c85:	64 69 6e 67 00 26 66 	imul   $0x8b662600,%fs:0x67(%esi),%ebp
    7c8c:	8b 
	# just an ELF format object, which doesn't have an
	# easy-to-read field to identify its own size (see [ELF1]).
	# But we limit Pintos kernels to 512 kB for other reasons, so
	# it's easy enough to just read the entire contents of the
	# partition or 512 kB from disk, whichever is smaller.
	mov %es:12(%si), %ecx		# EBP = number of sectors
    7c8d:	4c                   	dec    %esp
    7c8e:	0c 66                	or     $0x66,%al
	cmp $1024, %ecx			# Cap size at 512 kB
    7c90:	81 f9 00 04 00 00    	cmp    $0x400,%ecx
	jbe 1f
    7c96:	76 03                	jbe    7c9b <load_kernel+0x1d>
	mov $1024, %cx
    7c98:	b9 00 04 26 66       	mov    $0x66260400,%ecx
1:

	mov %es:8(%si), %ebx		# EBX = first sector
    7c9d:	8b 5c 08 b8          	mov    -0x48(%eax,%ecx,1),%ebx
	mov $0x2000, %ax		# Start load address: 0x20000
    7ca1:	00 20                	add    %ah,(%eax)

00007ca3 <next_sector>:

next_sector:
	# Read one sector into memory.
	mov %ax, %es			# ES:0000 -> load address
    7ca3:	8e c0                	mov    %eax,%es
	call read_sector
    7ca5:	e8 77 00 72 2d       	call   2d727d21 <__bss_start+0x2d71ff21>
	jc read_failed

	# Print '.' as progress indicator once every 16 sectors == 8 kB.
	test $15, %bl
    7caa:	f6 c3 0f             	test   $0xf,%bl
	jnz 1f
    7cad:	75 05                	jne    7cb4 <next_sector+0x11>
	call puts
    7caf:	e8 35 00 2e 00       	call   2e7ce9 <__bss_start+0x2dfee9>
	.string "."
1:

	# Advance memory pointer and disk sector.
	add $0x20, %ax
    7cb4:	83 c0 20             	add    $0x20,%eax
	inc %bx
    7cb7:	43                   	inc    %ebx
	loop next_sector
    7cb8:	e2 e9                	loop   7ca3 <next_sector>

	call puts
    7cba:	e8 2a 00 0d 00       	call   d7ce9 <__bss_start+0xcfee9>
#### registers, so in fact we store the address in a temporary memory
#### location, then jump indirectly through that location.  To save 4
#### bytes in the loader, we reuse 4 bytes of the loader's code for
#### this temporary pointer.

	mov $0x2000, %ax
    7cbf:	b8 00 20 8e c0       	mov    $0xc08e2000,%eax
	mov %ax, %es
	mov %es:0x18, %dx
    7cc4:	26 8b 16             	mov    %es:(%esi),%edx
    7cc7:	18 00                	sbb    %al,(%eax)
	mov %dx, start
    7cc9:	89 16                	mov    %edx,(%esi)
    7ccb:	d7                   	xlat   %ds:(%ebx)
    7ccc:	7c c7                	jl     7c95 <load_kernel+0x17>
	movw $0x2000, start + 2
    7cce:	06                   	push   %es
    7ccf:	d9 7c 00 20          	fnstcw 0x20(%eax,%eax,1)
	ljmp *start
    7cd3:	ff 2e                	ljmp   *(%esi)
    7cd5:	d7                   	xlat   %ds:(%ebx)
    7cd6:	7c                   	.byte 0x7c

00007cd7 <read_failed>:

read_failed:
start:
	# Disk sector read failed.
	call puts
    7cd7:	e8 0d 00 0d 42       	call   420d7ce9 <__bss_start+0x420cfee9>
    7cdc:	61                   	popa   
    7cdd:	64 20 72 65          	and    %dh,%fs:0x65(%edx)
    7ce1:	61                   	popa   
    7ce2:	64                   	fs
    7ce3:	0d                   	.byte 0xd
    7ce4:	00 cd                	add    %cl,%ch
1:	.string "\rBad read\r"

	# Notify BIOS that boot failed.  See [IntrList].
	int $0x18
    7ce6:	18                   	.byte 0x18

00007ce7 <puts>:
#### subroutine takes its null-terminated string argument from the
#### code stream just after the call, and then returns to the byte
#### just after the terminating null.  This subroutine preserves all
#### general-purpose registers.

puts:	xchg %si, %ss:(%esp)
    7ce7:	67 87 34             	xchg   %esi,(%si)
    7cea:	24 50                	and    $0x50,%al

00007cec <next_char>:
	push %ax
next_char:
	mov %cs:(%si), %al
    7cec:	2e 8a 04 46          	mov    %cs:(%esi,%eax,2),%al
	inc %si
	test %al, %al
    7cf0:	84 c0                	test   %al,%al
	jz 1f
    7cf2:	74 05                	je     7cf9 <next_char+0xd>
	call putc
    7cf4:	e8 08 00 eb f3       	call   f3eb7d01 <__bss_start+0xf3eaff01>
	jmp next_char
1:	pop %ax
    7cf9:	58                   	pop    %eax
	xchg %si, %ss:(%esp)
    7cfa:	67 87 34             	xchg   %esi,(%si)
    7cfd:	24 c3                	and    $0xc3,%al

00007cff <putc>:
#### [IntrList]).  Preserves all general-purpose registers.
####
#### If called upon to output a carriage return, this subroutine
#### automatically supplies the following line feed.

putc:	pusha
    7cff:	60                   	pusha  

1:	sub %bh, %bh			# Page 0.
    7d00:	28 ff                	sub    %bh,%bh
	mov $0x0e, %ah			# Teletype output service.
    7d02:	b4 0e                	mov    $0xe,%ah
	int $0x10
    7d04:	cd 10                	int    $0x10

	mov $0x01, %ah			# Serial port output service.
    7d06:	b4 01                	mov    $0x1,%ah
	sub %dx, %dx			# Serial port 0.
    7d08:	29 d2                	sub    %edx,%edx
2:	int $0x14			# Destroys AH.
    7d0a:	cd 14                	int    $0x14
	test $0x80, %ah			# Output timed out?
    7d0c:	f6 c4 80             	test   $0x80,%ah
	jz 3f
    7d0f:	74 06                	je     7d17 <putc+0x18>
	movw $0x9090, 2b		# Turn "int $0x14" above into NOPs.
    7d11:	c7 06 0a 7d 90 90    	movl   $0x90907d0a,(%esi)

3:
	cmp $'\r', %al
    7d17:	3c 0d                	cmp    $0xd,%al
	jne popa_ret
    7d19:	75 18                	jne    7d33 <popa_ret>
	mov $'\n', %al
    7d1b:	b0 0a                	mov    $0xa,%al
	jmp 1b
    7d1d:	eb e1                	jmp    7d00 <putc+0x1>

00007d1f <read_sector>:
#### reads the specified sector into memory at ES:0000.  Returns with
#### carry set on error, clear otherwise.  Preserves all
#### general-purpose registers.

read_sector:
	pusha
    7d1f:	60                   	pusha  
	sub %ax, %ax
    7d20:	29 c0                	sub    %eax,%eax
	push %ax			# LBA sector number [48:63]
    7d22:	50                   	push   %eax
	push %ax			# LBA sector number [32:47]
    7d23:	50                   	push   %eax
	push %ebx			# LBA sector number [0:31]
    7d24:	66 53                	push   %bx
	push %es			# Buffer segment
    7d26:	06                   	push   %es
	push %ax			# Buffer offset (always 0)
    7d27:	50                   	push   %eax
	push $1				# Number of sectors to read
    7d28:	6a 01                	push   $0x1
	push $16			# Packet size
    7d2a:	6a 10                	push   $0x10
	mov $0x42, %ah			# Extended read
    7d2c:	b4 42                	mov    $0x42,%ah
	mov %sp, %si			# DS:SI -> packet
    7d2e:	89 e6                	mov    %esp,%esi
	int $0x13			# Error code in CF
    7d30:	cd 13                	int    $0x13
	popa				# Pop 16 bytes, preserve flags
    7d32:	61                   	popa   

00007d33 <popa_ret>:
popa_ret:
	popa
    7d33:	61                   	popa   
	ret				# Error code still in CF
    7d34:	c3                   	ret    
	...
    7dfd:	00 55 aa             	add    %dl,-0x56(%ebp)
