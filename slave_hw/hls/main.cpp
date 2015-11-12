#include <ap_int.h>
#include <hls_stream.h>
#include <ap_axi_sdata.h> // For AXI-Stream side-channel signalling (i.e. TLAST)

#define DATAPATH_WIDTH 64

typedef ap_uint<DATAPATH_WIDTH> stream_word;
typedef ap_axiu<DATAPATH_WIDTH, 1, 1, 1> data_stream;

void top(hls::stream<data_stream>& in_intf, hls::stream<data_stream>& out_intf)
{
#pragma HLS INTERFACE axis port=in_intf
#pragma HLS INTERFACE axis port=out_intf
#pragma HLS INTERFACE ap_ctrl_none port=return

	// Receive and act on the data
	int headerBytes = 0;
	unsigned char destMac[6];
	unsigned char srcMac[6];
	unsigned char etherType[2];

	data_stream nextSendPkt;
	bool nextSendPktLast = false;

read_loop: while(1)
	{
		//#pragma HLS PIPELINE
		data_stream item;
		bool itemValid = in_intf.read_nb(item);

		// If we don't have a "next" packet, send the tail of the previous packet
		if(!itemValid && nextSendPktLast)
		{
			nextSendPktLast = false;
			out_intf.write(nextSendPkt);
		}
		else if(itemValid) // We have a valid packet
		{
			if(headerBytes == 0) // destination MAC and part of source MAC
			{
				if(item.last)
				{
					// Early termination from master, reset
					headerBytes = 0;
				}
				else
				{
					destMac[0] = (item.data >> 0)  & 0xFF;
					destMac[1] = (item.data >> 8)  & 0xFF;
					destMac[2] = (item.data >> 16) & 0xFF;
					destMac[3] = (item.data >> 24) & 0xFF;
					destMac[4] = (item.data >> 32) & 0xFF;
					destMac[5] = (item.data >> 40) & 0xFF;
					srcMac[0] = (item.data >> 48) & 0xFF;
					srcMac[1] = (item.data >> 56) & 0xFF;
					headerBytes++;

					// We have a new packet, and the tail of the last packet to send.
					if(nextSendPktLast)
					{
						nextSendPktLast = false;
						out_intf.write(nextSendPkt);
					}
				}
			}
			else if(headerBytes == 1)
			{
				if(item.last)
				{
					// Again, invalid. Reset.
					headerBytes = 0;
				}
				else
				{
					srcMac[2] = (item.data >> 0)  & 0xFF;
					srcMac[3] = (item.data >> 8)  & 0xFF;
					srcMac[4] = (item.data >> 16) & 0xFF;
					srcMac[5] = (item.data >> 24) & 0xFF;

					// Get the ethertype
					etherType[0] = (item.data >> 32) & 0xFF;
					etherType[1] = (item.data >> 40) & 0xFF;

					unsigned char data_tmp[2];
					data_tmp[0] = (item.data >> 48) & 0xFF;
					data_tmp[1] = (item.data >> 56) & 0xFF;

					data_tmp[0] += 0xAA;
					data_tmp[1] += 0xAA;

					// Build the current packet
					data_stream sendPkt = {0};
					sendPkt.data = (stream_word)srcMac[0];
					sendPkt.data |= (stream_word)srcMac[1] << 8;
					sendPkt.data |= (stream_word)srcMac[2] << 16;
					sendPkt.data |= (stream_word)srcMac[3] << 24;
					sendPkt.data |= (stream_word)srcMac[4] << 32;
					sendPkt.data |= (stream_word)srcMac[5] << 40;
					sendPkt.data |= (stream_word)destMac[0] << 48;
					sendPkt.data |= (stream_word)destMac[1] << 56;

					sendPkt.keep = 0xFF; // All bytes are valid
					sendPkt.strb = 0xFF;
					out_intf.write(sendPkt);

					// Build the next packet to send
					nextSendPkt.data = (stream_word)destMac[2];
					nextSendPkt.data |= (stream_word)destMac[3] << 8;
					nextSendPkt.data |= (stream_word)destMac[4] << 16;
					nextSendPkt.data |= (stream_word)destMac[5] << 24;
					nextSendPkt.data |= (stream_word)etherType[0] << 32;
					nextSendPkt.data |= (stream_word)etherType[1] << 40;
					nextSendPkt.data |= (stream_word)data_tmp[0] << 48;
					nextSendPkt.data |= (stream_word)data_tmp[1] << 56;

					nextSendPkt.last = 0; // Ensure that the previous contents of nextSendPkt don't assert last
					nextSendPkt.strb = 0xFF;
					nextSendPkt.keep = 0xFF;
					nextSendPkt.id = 0;
					nextSendPkt.user = 0;
					nextSendPkt.dest = 0;

					headerBytes++;
				}
			}
			else
			{
				// Simply send the data back verbatim
				out_intf.write(nextSendPkt);

				if(item.last)
				{
					nextSendPktLast = true;
					headerBytes = 0;
				}

				nextSendPkt = item;
				nextSendPkt.id = 0;
				nextSendPkt.user = 0;
				nextSendPkt.dest = 0;
			}
		}
	}
}
