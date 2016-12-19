/*
    This file is part of PiCo.
    PiCo is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
    PiCo is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License
    along with PiCo.  If not, see <http://www.gnu.org/licenses/>.
*/
/*
 * WriteToDiskFFNodeMB.hpp
 *
 *  Created on: Dec 9, 2016
 *      Author: misale
 */

#ifndef INTERNALS_FFOPERATORS_INOUT_WRITETODISKFFNODEMB_HPP_
#define INTERNALS_FFOPERATORS_INOUT_WRITETODISKFFNODEMB_HPP_

#include "ff/node.hpp"
#include "../../utils.hpp"

using namespace ff;

template <typename In>
class WriteToDiskFFNodeMB: public ff_node{
public:
	WriteToDiskFFNodeMB(std::function<std::string(In)> kernel_, std::string filename_):
			kernel(kernel_), filename(filename_), microbatch(nullptr), recv_sync(false){};

	int svc_init(){
		outfile.open(filename);
		return 0;
	}
	void* svc(void* task){
		if(task != PICO_EOS && task != PICO_SYNC){
			microbatch = reinterpret_cast<std::vector<In>*>(task);
			if (outfile.is_open()) {
				for(In in: *microbatch){
					outfile << kernel(in)<< std::endl;
					//delete in;
				}
			} else {
				std::cerr << "Unable to open file";
			}
		}
//		delete microbatch;
		return GO_ON;
	}

	void svc_end(){
		outfile.close();
	}

private:
	std::function<std::string(In)> kernel;
    std::string filename;
    std::ofstream outfile;
    std::vector<In>* microbatch;
    bool recv_sync;
};



#endif /* INTERNALS_FFOPERATORS_INOUT_WRITETODISKFFNODEMB_HPP_ */
