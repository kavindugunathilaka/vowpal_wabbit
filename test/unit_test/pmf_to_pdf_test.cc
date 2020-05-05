#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>
#include <iostream>
#include "learner.h"
#include "pmf_to_pdf.h"
#include "parse_args.h"  // setup_base()
#include "action_score.h"

using namespace LEARNER;
using namespace std;


namespace VW { namespace pmf_to_pdf {

void learn(VW::pmf_to_pdf::reduction& data, single_learner& base, example& ec);
void predict(VW::pmf_to_pdf::reduction& data, single_learner& base, example& ec);

struct reduction_test_harness
{
  reduction_test_harness() : _curr_idx(0) {}

  void set_predict_response(const vector<pair<uint32_t, float>>& predictions) { _predictions = predictions; }
  
  void test_predict(single_learner& base, example& ec)
  {
    ec.pred.a_s.clear();
    for (uint32_t i = 0; i < _predictions.size(); i++)
    {
      ec.pred.a_s.push_back(ACTION_SCORE::action_score{_predictions[i].first, _predictions[i].second});
    }

    cout << "\nec.pred.a_s (PMF): " << endl;
    for (uint32_t i = 0; i < _predictions.size(); i++)
    {
      cout << "(" << ec.pred.a_s[i].action << " : " << ec.pred.a_s[i].score << "), " << endl;
    }
  }

  void test_learn(single_learner& base, example& ec)
  {
    cout << "ec.l.cb.costs after:" << endl;
    for (uint32_t i = 0; i < ec.l.cb.costs.size(); i++) 
    {
      cout << "(" << ec.l.cb.costs[i].action << " , " << ec.l.cb.costs[i].cost << " , " << ec.l.cb.costs[i].probability
         << " , " << ec.l.cb.costs[i].partial_prediction << "), " << endl;
    }    
  }

  static void predict(reduction_test_harness& test_reduction, single_learner& base, example& ec)
  {
    test_reduction.test_predict(base, ec);
  }

  static void learn(reduction_test_harness& test_reduction, single_learner& base, example& ec)
  {
    test_reduction.test_learn(base, ec);
  };

 private:
  vector<pair<uint32_t, float>> _predictions;
  int _curr_idx;
};

using test_learner_t = learner<reduction_test_harness, example>;
using predictions_t = vector<pair<uint32_t, float>>;

test_learner_t* get_test_harness_reduction(const predictions_t& base_reduction_predictions);

}  // namespace pmf_to_pdf
}  // namespace VW

BOOST_AUTO_TEST_CASE(pmf_to_pdf_basic)
{
  uint32_t k = 4;
  uint32_t h = 1;
  float min_val = 1000;
  float max_val = 1100;

  const VW::pmf_to_pdf::predictions_t prediction_scores {{2,1.f}};

  const auto test_harness = VW::pmf_to_pdf::get_test_harness_reduction(prediction_scores);

  example ec;
  
  auto data = scoped_calloc_or_throw<VW::pmf_to_pdf::reduction>();
  data->num_actions = k;
  data->bandwidth = h;
  data->min_value = min_val;
  data->max_value = max_val;
  data->_p_base = as_singleline(test_harness);

  ec.pred.a_s = v_init<ACTION_SCORE::action_score>();

  predict(*data, *data->_p_base, ec);

  float sum = 0;
  cout << "ec.pred.p_d (PDF): " << endl;
  for (uint32_t i = 0; i < ec.pred.prob_dist.size(); i++)
  {
    cout << "(" << ec.pred.prob_dist[i].left << " , " << ec.pred.prob_dist[i].right << 
    ": " << ec.pred.prob_dist[i].pdf_value << ")" << endl;
    sum += ec.pred.prob_dist[i].pdf_value * (ec.pred.prob_dist[i].right - ec.pred.prob_dist[i].left);
  }
  cout << "sum = " << sum << endl;

  ec.l.cb_cont = VW::cb_continuous::continuous_label();
  ec.l.cb_cont.costs = v_init<VW::cb_continuous::continuous_label_elm>();
  ec.l.cb_cont.costs.clear();
  ec.l.cb_cont.costs.push_back({1010.17f, .5f, .05f, 0.f}); // action, cost, prob, partial

  cout << "ec.l.cb_cont.costs after:" << endl;
  cout << "(" << ec.l.cb_cont.costs[0].action << " , " << ec.l.cb_cont.costs[0].cost << " , " << ec.l.cb_cont.costs[0].probability
    << " , " << ec.l.cb_cont.costs[0].partial_prediction << "), " << endl;

  learn(*data, *as_singleline(test_harness), ec);

  float chosen_action = 1080; 
  cout << "pdf value of " << chosen_action << " is = " << VW::actions_pdf::get_pdf_value(ec.pred.prob_dist, chosen_action)
       << std::endl;
  cout << "here" << endl;
}

namespace VW { namespace pmf_to_pdf {
test_learner_t* get_test_harness_reduction(const predictions_t& base_reduction_predictions)
{
  // Setup a test harness base reduction
  auto test_harness = scoped_calloc_or_throw<reduction_test_harness>();
  test_harness->set_predict_response(base_reduction_predictions);
  auto& test_learner =
      init_learner(test_harness,          // Data structure passed by vw_framework into test_harness predict/learn calls
          reduction_test_harness::learn,  // test_harness learn
          reduction_test_harness::predict,  // test_harness predict
          1                                 // Number of regressors in test_harness (not used)
      );                                    // Create a learner using the base reduction.
  return &test_learner;
}
}}  // namespace VW::pmf_to_pdf