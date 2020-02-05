#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/stat.h>

#include <boost/utility.hpp>
#include <boost/format.hpp>
#include <boost/spirit/home/x3.hpp>

#include <fstream>
#include <iterator>
#include <iostream>
#include <vector>
#include <list>
#include <sstream>
#include <string>
#include <vector>
#include <set>

using namespace std;
namespace x3 = boost::spirit::x3;


const int TMP_BUF_LEN = 512;

void debug();
void debug_set(const char *file, int line);
void debug_print(const char *fmt, ... );
#define DEBUG_PRT (debug_set(__FILE__,__LINE__), debug_print)	
void PRT(const char *fmt, ...);


void debug() { exit(0); }
void PRT(const char *fmt, ...)
{
	va_list arg;
	va_start( arg, fmt );
	vfprintf( stderr, fmt, arg );
	va_end( arg );
}

static char debug_str[TMP_BUF_LEN];

void debug_set(const char *file, int line)
{
	char str[TMP_BUF_LEN];
	strncpy(str, file, TMP_BUF_LEN-1);
	const char *p = strrchr(str, '\\');
	if ( p == NULL ) p = file;
	else p++;
	sprintf(debug_str,"%s Line %d\n\n",p,line);
}

void debug_print(const char *fmt, ... )
{
	va_list ap;
	static char text[TMP_BUF_LEN];
	va_start(ap, fmt);
#if defined(_MSC_VER)
	_vsnprintf( text, TMP_BUF_LEN-1, fmt, ap );
#else
	 vsnprintf( text, TMP_BUF_LEN-1, fmt, ap );
#endif
	va_end(ap);
	static char text_out[TMP_BUF_LEN*2];
	sprintf(text_out,"%s%s",debug_str,text);
	PRT("%s\n",text_out);
	debug();
}


const int WT_AVE_NUM = 10;

const int WT_NUM = 23425624;
std::vector<float> wt_sum(WT_NUM);


std::pair<int, int> load_v1_network(std::istream& wtfile, int wt_ave_loop)
{
	FILE *fp = NULL;
	if ( wt_ave_loop == WT_AVE_NUM - 1 ) {
		fp = fopen("we_ave.txt","w");
		fprintf(fp,"2\n");
	}

    // Count size of the network
    PRT("Detecting residual layers...");

    // First line was the version number
    auto linecount = size_t{1};
    auto channels = 0;
    auto line = std::string{};
    while (std::getline(wtfile, line)) {
        auto iss = std::stringstream{line};
        // Third line of parameters are the convolution layer biases,
        // so this tells us the amount of channels in the residual layers.
        // We are assuming all layers have the same amount of filters.
        if (linecount == 2) {
            auto count = std::distance(std::istream_iterator<std::string>(iss),
                                       std::istream_iterator<std::string>());
            PRT("%d channels...", count);
            channels = count;
        }
        linecount++;
    }
    // 1 format id, 1 input layer (4 x weights), 14 ending weights,
    // the rest are residuals, every residual has 8 x weight lines
    auto residual_blocks = linecount - (1 + 4 + 14);
    if (residual_blocks % 8 != 0) {
        PRT("\nInconsistent number of weights in the file.\n");
        return {0, 0};
    }
	PRT("linecount=%d...",linecount);
    residual_blocks /= 8;
    PRT("%d blocks.\n", residual_blocks);

    // Re-read file and process
    wtfile.clear();
    wtfile.seekg(0, std::ios::beg);


    // Get the file format id out of the way
    std::getline(wtfile, line);

    const auto plain_conv_layers = 1 + (residual_blocks * 2);
    const auto plain_conv_wts = plain_conv_layers * 4;
    linecount = 0;
    int num = 0;
    while (std::getline(wtfile, line)) {
        std::vector<float> weights;
        auto it_line = line.cbegin();
        const auto ok = phrase_parse(it_line, line.cend(),  *x3::float_, x3::space, weights);
		int n = weights.size();
        for (int i=0; i<n; i++) wt_sum[num+i] += weights[i];
        if ( fp ) {
			for (int i=0; i<n; i++) {
				if ( i>0 ) fprintf(fp," ");
				fprintf(fp,"%f",wt_sum[num+i] / WT_AVE_NUM);
			}
			fprintf(fp,"\n");
		}
        num += n;
        PRT("%3d:n=%8d:num=%10d\n",linecount,n,num);
        
        if (!ok || it_line != line.cend()) {
            PRT("\nFailed to parse weight file. Error on line %d.\n",
                    linecount + 2); //+1 from version line, +1 from 0-indexing
            return {0, 0};
        }
        if (linecount < plain_conv_wts) {
            if (linecount % 4 == 0) {
//                m_fwd_weights->m_conv_weights.emplace_back(weights);
            } else if (linecount % 4 == 1) {
                // Redundant in our model, but they encode the
                // number of outputs so we have to read them in.
//                m_fwd_weights->m_conv_biases.emplace_back(weights);
            } else if (linecount % 4 == 2) {
//				modify_bn_scale_factor(weights);
//               m_fwd_weights->m_batchnorm_means.emplace_back(weights);
            } else if (linecount % 4 == 3) {
//				modify_bn_scale_factor(weights);
//                process_bn_var(weights);
//                m_fwd_weights->m_batchnorm_stddevs.emplace_back(weights);
            }
        } else {
            switch (linecount - plain_conv_wts) {
//              case  0: m_fwd_weights->m_conv_pol_w = std::move(weights); break;
            }
        }
        linecount++;
    }
	PRT("num=%d...linecount=%d\n",num,linecount);
	if ( num != WT_NUM ) debug();
	for (int i=0; i<100; i++) PRT("%f,",wt_sum[i]);

    return {channels, static_cast<int>(residual_blocks)};
}

std::pair<int, int> load_network_file(std::string filename, int wt_ave_loop)
{
	ifstream wtfile;
//  auto wtfile = std::ifstream{filename};
	wtfile.open(filename);
		 
    if (wtfile.fail()) {
        PRT("Could not open weights file: %s\n", filename.c_str());
        return {0, 0};
    }

    // Read format version
    auto line = std::string{};
    auto format_version = -1;
    if (std::getline(wtfile, line)) {
        auto iss = std::stringstream{line};
        // First line is the file format version id
        iss >> format_version;
        if (iss.fail() || format_version != 2 ) {
            PRT("Weights file is the wrong version.\n");
            return {0, 0};
        } else {
            assert(format_version == 2 );
            return load_v1_network(wtfile, wt_ave_loop);
        }
    }

    return {0, 0};
}

int main(int argc, char *argv[])
{
	int i;
	for (i=0;i<WT_AVE_NUM;i++) {
		char filename[TMP_BUF_LEN];
		sprintf(filename,"/home/yss/test/extract/w%012d.txt",i+848);
		PRT("filename=%s\n",filename);
		load_network_file(filename,i);
	}

	return 0;
}
