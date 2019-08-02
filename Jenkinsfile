/*
This is a basic Jenkinsfile written in declarative pipeline syntax.
This job runs a docker command on jaas docker agent
*/
library identifier: 'blp-dpkg-jaas-library@master',
  retriever: modernSCM([
    $class: 'GitSCMSource',
    credentialsId: 'bbgithub_token',
    remote: 'https://bbgithub.dev.bloomberg.com/apopov49/blp-dpkg-jaas-library'
  ])

pipeline {
    agent none
    options {
        timeout(time:30, unit: 'MINUTES')                   // stops job if passed 10 minutes
        buildDiscarder(logRotator(numToKeepStr: '15'))      // only keeping 15 builds
        disableConcurrentBuilds()                           // do not allow concurrent build of this job.
							    // needed for docker-compose, to prevent network conflict
    }
    stages{
        stage('build'){
	when { not { branch 'master' } } 
        parallel {
            stage('local build') { 
                agent { label 'BLDLNX' }
                steps{
		    sh './corokafka/tests/jenkins/local_build.sh'
		    stash includes: 'build/', name: 'app'
                }
	        post {
                    always {
                        deleteDir()
                    }
	        }
            }
        
            stage('dpkg build') { 
                agent { label 'BLDLNX' }
                steps{
		    blpDpkgBuildSinglePackage(scm: scm, nodes: [ "BLDLNX" ])
                }
	        post {
                    always {
                        deleteDir()
                    }
	        }
            }
        }
        }

        stage('test') { 
	    when { not { branch 'master' } } 
            agent { label 'docker' }
            steps{
		unstash 'app'
		sh './corokafka/tests/jenkins/run_tests.sh'
            }
	    post {
                always {
                    deleteDir()
                }
	    }
        }
	
        stage('dpkg') {
	    when { branch 'master' } 
            agent { label 'BLDLNX' }
            steps{
                 blpDpkgPromoteSinglePackage(scm: scm,
                                            branch: "master",
                                            distribution: "unstable",
                                            credentials: "bbgh_bbgithub_token",
                                            cancel_build: true)
            }
	    post {
                always {
                    deleteDir()
                }
	    }
        }
    }
}
