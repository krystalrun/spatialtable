#include <iostream>
#include <map>
#include <vector>
#include <rpcz/rpcz.hpp>
#include "../common/gen/tabletserver.pb.h"
#include "../common/gen/tabletserver.rpcz.h"
#include "../common/utils.h"
#include "tablet.h"

using namespace std;

std::map<string,tablet*> tablets;

typedef tablet* (*TabletFactory)(const std::string&);

TabletFactory tabletFactories[9];

template<int N>
struct tabletFactoryInitializer {
  static void initialize() {
    tabletFactories[N]=(tablet::template New<N>);
    tabletFactoryInitializer<N-1>::initialize();
  }
};
template<>
struct tabletFactoryInitializer<0> {
  static void initialize() {
  }
};

class TabletServerServiceImpl : public TabletServerService {
  virtual void CreateTable(const Table& request, rpcz::reply<Status> reply) {
    Status response;
    int dim = request.dim();
    if (dim<=0 || dim>=9) {
      response.set_status(Status::WrongDimension);
      reply.send(response);
      return;
    }
    tablet* t = tabletFactories[dim](request.name());
    tablets[t->get_name()]=t;
    response.set_status(Status::Success);
    reply.send(response);
  }

  virtual void Insert(const InsertRequest& request, rpcz::reply<Status> reply) {
    Status response;
    auto it = tablets.find(request.tablet());
    if (it==tablets.end()) {
      response.set_status(Status::NoSuchTablet);
      reply.send(response);
      return;
    }
    int dim = it->second->get_dim();
    if (request.data().box().start_size()!=dim || request.data().box().end_size()!=dim) {
      response.set_status(Status::WrongDimension);
      reply.send(response);
      return;
    }
    it->second->insert(request.data().box(), request.data().value());
    response.set_status(Status::Success);
    reply.send(response);
  }

  virtual void Query(const QueryRequest& request, rpcz::reply<QueryResponse> reply) {
    QueryResponse response;
    auto it = tablets.find(request.tablet());
    if (it==tablets.end()) {
      response.mutable_status()->set_status(Status::NoSuchTablet);
      reply.send(response);
      return;
    }
    int dim = it->second->get_dim();
    if (request.query().start_size()!=dim || request.query().end_size()!=dim) {
      response.mutable_status()->set_status(Status::WrongDimension);
      reply.send(response);
      return;
    }
    it->second->query(request.query(), request.is_within(), response);
    response.mutable_status()->set_status(Status::Success);    
    reply.send(response);
  }

  virtual void ListTablets(const ListRequest& request, rpcz::reply<ListResponse> reply) {
    ListResponse response;
    for (auto i : tablets) {
      TabletDescription* t = response.add_results();
      t->set_name(i.second->get_name());
      t->set_dim(i.second->get_dim());      
    }
    reply.send(response);
  }

};

int main() {
  tabletFactoryInitializer<8>::initialize();
  rpcz::application application;
  rpcz::server server(application);
  TabletServerServiceImpl tabletserver_service;
  server.register_service(&tabletserver_service);
  cout << "Serving requests on port 5555." << endl;
  server.bind("tcp://*:5555");
  application.run();
}