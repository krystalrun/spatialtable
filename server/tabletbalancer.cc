#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <rpcz/rpcz.hpp>
#include "../common/gen/tabletserver.pb.h"
#include "../common/gen/tabletserver.rpcz.h"
#include "../common/utils.h"
#include "../common/client/libclient.h"

using namespace std;
rpcz::application application;


int main(int argc, char ** argv) {
  vector<string> server = {"128.59.146.138:5555", "128.59.150.196:5555","128.59.46.146:5555","128.59.149.122:5555"};
  vector<int> serverTotalLoad = {0, 0, 0, 0};
  vector<int> serverTotalTablets = {0, 0, 0, 0};
  vector<ListResponse> tablets(4);
  vector<TabletServerService_Stub*> stubs(4);
  for (int i = 0; i < 4; i++) {
    stubs[i] = new TabletServerService_Stub (application.create_rpc_channel("tcp://"+server[i]));
  }

  while (1){
    for (int i = 0; i < 4; i++) {
      ListRequest request;
      try {
        stubs[i]->ListTablets(request, &tablets[i], 1000);
        serverTotalTablets[i] = tablets[i].results_size();
        for (int j = 0; j < tablets[i].results_size(); j++){
          serverTotalLoad[i] += tablets[i].results(j).size();
        }
       } 
       catch (rpcz::rpc_error &e) {
        cout << "Error: " << e.what() << endl;;     
      }
      cout << "Server: "<< server[i] <<"\tTablets loaded: " << serverTotalTablets[i] <<"\tRows Loaded: " << serverTotalLoad[i] << endl;
    }

    int min = 0;
    int max = 0;
    int maxval = -1;
    int minval = 1<<30;
    for (int i=0; i < 4; i++){
      if (serverTotalLoad[i] < minval) {
        min = i;
        minval = serverTotalLoad[i];
      }
      if (serverTotalLoad[i] > maxval) {
        max = i;
        maxval = serverTotalLoad[i];
      }
    }

    cout << "Max load server: " << server[max] << " Rows loaded: " << serverTotalLoad[max]  << endl;
    cout << "Min load server: " << server[min] << " Rows loaded: " << serverTotalLoad[min]  << endl;
    int numRowsToMove; 

    if (serverTotalLoad[max] == 0) {
         numRowsToMove = 0;
    } else {
       numRowsToMove = (serverTotalLoad[max] - serverTotalLoad[min])/2;
       //cout << "Rows to move:  " << numRowsToMove << endl;
    }
    int diff = 0;
    int minDiff = 0;
    int tabletToMove = -1;
    cout << "Tablet to Move: " << tabletToMove <<" Rows to move:  " << numRowsToMove << endl;

    if (numRowsToMove != 0) {
      for (int i=0; i< tablets[max].results_size(); i++){
        if ((numRowsToMove >= tablets[max].results(i).size()) ){
          if (minDiff <= tablets[max].results(i).size()){
            minDiff = tablets[max].results(i).size();
            tabletToMove = i;
          }
        } 
        else {
          continue;
        }       
      }
      if(tabletToMove > -1){ 
        cout <<"Moving tablet: " << tablets[max].results(tabletToMove).name() << " (" << tabletToMove <<") from " << server[max] << " to " << server[min] << endl; 
        UnLoadRequest request;
        Status response;
        request.set_tablet(tablets[max].results(tabletToMove).name());
        try{
          stubs[max]->UnLoadTablet(request, &response, 5000);
          cout << "Unloaded... " << Status::StatusValues_Name(response.status()) << endl;
        } 
        catch (rpcz::rpc_error &e) {
          cout << "Error: " << e.what() << endl;;
    		  exit(1);
        }
        LoadRequest request2;
        Status response2;
        request2.set_tablet(tablets[max].results(tabletToMove).name());
        request2.set_dim(tablets[max].results(tabletToMove).dim());
        try {
          stubs[min]->LoadTablet(request2, &response2, 5000);
          cout << "Loaded... " << Status::StatusValues_Name(response2.status()) << endl;
        } 
        catch (rpcz::rpc_error &e) {
          cout << "Error: " << e.what() << endl;
    		  exit(1);
        }
      } else { 
          sleep(5);
        }
      // clear totals
      serverTotalLoad = {0, 0, 0, 0};
      numRowsToMove = 0;
      serverTotalTablets = {0, 0, 0, 0};
      cout << "******** Iteration done **********" << endl;
            
    } else{
      // clear totals
      cout << "******** Nothing to do ***********" << endl;
      serverTotalLoad = {0, 0, 0, 0};
      numRowsToMove = 0;
      serverTotalTablets = {0, 0, 0, 0};
      sleep(5);
    }
  } 
}
