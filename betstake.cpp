
#include <string>

#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>

#include <eosiolib/multi_index.hpp>

#include <eosio.token/eosio.token.hpp>



#include <eosiolib/print.hpp>
#include <eosiolib/datastream.hpp>
#include <eosiolib/serialize.hpp>

#include <eosiolib/privileged.h>
#include <eosiolib/transaction.hpp>




#include <cmath>
#include <map>


using eosio::asset;

using namespace eosio;


#define TOKENHOLDER N(helloworld11) // owner of token contract

class betstake : public eosio::contract {

    static constexpr uint32_t refund_delay_sec = 24 * 3600;



    private:

        /// @abi table unstaking
        struct unstaking {
            account_name    owner;
            asset           amount;
            time  			request_time;

            uint64_t primary_key()const { return owner; }

            EOSLIB_SERIALIZE( unstaking, (owner)(amount)(request_time) )
        };


        /// @abi table account i64
        struct account {
            account_name owner;
            asset        balance;

            bool is_empty() const { return !(balance.amount); }
            uint64_t primary_key() const { return owner; }

            EOSLIB_SERIALIZE( account, (owner)(balance) )
        };

        typedef eosio::multi_index< N(unstaking), unstaking> unstaking_index;
        typedef eosio::multi_index< N(account), account> account_index;

        account_index   accounts;
        unstaking_index unstakes;



    public:
	
		betstake(account_name self):eosio::contract(self),
			// 初始化列表
			accounts(_self, _self),
			unstakes(_self, _self)
		{}
		
        // @abi action
        void transfer(const account_name from, const account_name to, const asset& quantity, const std::string& memo) {

            if (from == _self || to != _self) {
				return;
			}

            eosio_assert( quantity.is_valid(), "Invalid asset");
            eosio_assert( quantity.amount > 0, "must stake positive quantity");

            auto itr = accounts.find(from);
            if( itr == accounts.end() ) {
                itr = accounts.emplace(_self, [&](auto& acnt){
                    acnt.owner = from;
                });
            };

            accounts.modify( itr, 0, [&]( auto& acnt ) {
                acnt.balance += quantity;
            });
        }

        // @abi action
        void unstfake(const account_name to, const asset& quantity) {
            require_auth(to);

            eosio_assert( quantity.is_valid(), "Invalid asset");
            eosio_assert( quantity.amount > 0, "must stake positive quantity");

            auto itr = accounts.find(to);
            eosio_assert(itr != accounts.end(), "unknown account");

            accounts.modify(itr, 0, [&](auto& acnt) {
                eosio_assert(acnt.balance >= quantity, "insufficient balance");
                acnt.balance -= quantity;
            });

            auto req_itr = unstakes.find(to);
            if(req_itr == unstakes.end()) {
                req_itr = unstakes.emplace(_self, [&](auto& acnt){
                    acnt.owner = to;
                });
            };

            unstakes.modify(req_itr, 0, [&](auto& acnt){
                acnt.amount += quantity;
            });

            eosio::transaction txn{};
            txn.actions.emplace_back(
                eosio::permission_level(_self, N(active)),
                _self,
                N(refund),
                std::make_tuple(to)
            );
            txn.delay_sec = 86400;
            txn.send(to, to,true);

            if(itr->is_empty()) {
                accounts.erase(itr);
            };          
        }
		
		// @abi action
        void refund(const account_name owner) {
            require_auth(owner);

            auto req = unstakes.find(owner);
            eosio_assert(req != unstakes.end(), "refund request not found");
            //eosio_assert(req->request_time + refund_delay_sec <= time_point_sec(now()), "refund is not available yet");

            action(
                permission_level{_self, N(active)},
                TOKENHOLDER,
                N(transfer),
                std::make_tuple(
                    _self,
                    N(owner),
                    req->amount,
                    ""
                )
            ).send();

            unstakes.erase(req);
        }
};

#define EOSIO_ABI_EX( TYPE, MEMBERS ) \
extern "C" { \
   void apply( uint64_t receiver, uint64_t code, uint64_t action ) { \
      auto self = receiver; \
      if( code == self || code == TOKENHOLDER) { \
      	 if( action == N(transfer)){ \
      	 	eosio_assert( code == TOKENHOLDER, "Must transfer PLY"); \
      	 } \
         TYPE thiscontract( self ); \
         switch( action ) { \
            EOSIO_API( TYPE, MEMBERS ) \
         } \
         /* does not allow destructor of thiscontract to run: eosio_exit(0); */ \
      } \
   } \
}

EOSIO_ABI_EX( betstake, (transfer)(unstfake) )
