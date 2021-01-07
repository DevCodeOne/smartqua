import React, { Component } from 'react'

import { Typography, Grid } from '@material-ui/core'
import { ApiService } from './ApiService.js'

export class Home extends Component {
    constructor(props) {
        super(props);
        this.state = { currentLoad : "0g", containedCo2 : "0g"};
    }

    componentDidMount() {
        this.timer = setInterval( () => {
            const data = ApiService.GetLoad();
            data.then(data => this.setState( { currentLoad : data.load + "g", containedCo2 : (data.contained_co2 - (Math.abs(data.load))) + "g"}))
        }, 1000);
    }

    componentWillUnmount() {
        clearInterval(this.timer);
    }

    render() {
      return (
        <div>
          <Grid container direction="row" justify="center" alignItems="center" spacing={4}>
            <Grid item xs={12}>
              <Typography variant="h2" align="Center">
                Co2 Usage
              </Typography>
            </Grid>
            <Grid item xs={12}>
              <Typography variant="h3" align="center">
                Still contained Co2 : {this.state.containedCo2}
              </Typography>
            </Grid>
            <Grid item xs={12}>
              <Typography variant="h3" align="Center">
                Weight difference : {this.state.currentLoad}
              </Typography>
            </Grid>
          </Grid>
        </div>
      );
    }
}